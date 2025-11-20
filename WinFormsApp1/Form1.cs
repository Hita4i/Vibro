using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Windows.Forms;

#nullable disable

namespace X8_Control_panel
{
    public class CompactForm : Form
    {
        private SerialPort sp;
        private readonly ComboBox cmbPort = new();
        private readonly TrackBar tbSpeed = new();
        private readonly Label lblSpeed = new();
        private readonly Label lblStatus = new();
        private readonly Button btnLaunch = new();
        private readonly DataGridView dgvSchedule = new();

        private readonly System.Windows.Forms.Timer timerMain = new();
        private readonly System.Windows.Forms.Timer timerRead = new();

        private List<ScheduleItem> schedule = new();
        private int currentStep = 0;
        private int remainingSeconds = 0;

        public CompactForm()
        {
            Text = "X8 - ПАНЕЛЬ";
            Size = new Size(720, 750);                    // зменшено для будь-якого екрану
            FormBorderStyle = FormBorderStyle.FixedSingle;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(34, 34, 34);
            ForeColor = Color.Cyan;
            DoubleBuffered = true;

            // === ПРАПОР ===
            var pbFlag = new PictureBox
            {
                Dock = DockStyle.Top,
                Height = 70,
                Image = CreateSmallFlag(),
                SizeMode = PictureBoxSizeMode.StretchImage
            };

            // === ПОРТ ===
            var pnlPort = new Panel { Dock = DockStyle.Top, Height = 60, BackColor = Color.FromArgb(34, 34, 34) };
            pnlPort.Controls.Add(new Label 
            { 
                Text = "ПОРТ:", 
                Location = new Point(5, 16), 
                ForeColor = Color.FromArgb(206, 145, 120), 
                Font = new Font("Segoe UI", 11F, FontStyle.Bold),
                AutoSize = true
            });

            cmbPort.Location = new Point(60, 16);
            cmbPort.Size = new Size(130, 30);
            cmbPort.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbPort.BackColor = Color.DarkGray;
            cmbPort.ForeColor = Color.Black;
            cmbPort.FlatStyle = FlatStyle.Flat;
            pnlPort.Controls.Add(cmbPort);

            var btnRefresh = new Button { Text = "↻", Location = new Point(200, 12), Size = new Size(40, 30), BackColor = Color.FromArgb(206, 145, 120), ForeColor = Color.Black, FlatStyle = FlatStyle.Flat };
            btnRefresh.Click += (s, e) => RefreshPorts();
            pnlPort.Controls.Add(btnRefresh);

            var btnConnect = new Button { Text = "ПІДКЛЮЧИТИ", Location = new Point(250, 12), Size = new Size(110, 30), BackColor = Color.FromArgb(206, 145, 120), ForeColor = Color.Black, FlatStyle = FlatStyle.Flat };
            btnConnect.Click += (s, e) => ToggleConnect(btnConnect);
            pnlPort.Controls.Add(btnConnect);

            // === КНОПКИ ШВИДКОСТІ ===
            var pnlButtons = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 80, Padding = new Padding(10, 5, 10, 5) };
            foreach (int s in new[] { 0, 25, 50, 75, 100 })
            {
                var btn = new Button
                {
                    Text = s == 0 ? "СТОП" : $"{s}",
                    Size = new Size(65, 60),                     // зменшено
                    Font = new Font("Segoe UI", 12F, FontStyle.Bold),
                    BackColor = s == 0 ? Color.DarkRed : Color.FromArgb(206, 145, 120),
                    ForeColor = Color.WhiteSmoke,
                    FlatStyle = FlatStyle.Flat,
                    FlatAppearance = { BorderSize = 2, BorderColor = Color.WhiteSmoke }
                };
                btn.Click += (se, ev) => Send($"SPEED:{s}");
                pnlButtons.Controls.Add(btn);
            }

            // === АВАРІЙКА — ТЕПЕР КОМПАКТНІША ===
            var btnEmergency = new Button
            {
                Text = "АВАРІЙКА",
                Dock = DockStyle.Top,
                Height = 50,
                Font = new Font("Segoe UI", 18F, FontStyle.Bold),
                BackColor = Color.DarkRed,
                ForeColor = Color.WhiteSmoke,
                FlatStyle = FlatStyle.Flat
            };
            btnEmergency.Click += (s, e) =>
            {
                StopAll();
                Send("SPEED:0");
                FlashRed();
                System.Media.SystemSounds.Hand.Play();
            };

            // === БІГУНОК + ШВИДКІСТЬ ===
            tbSpeed.Dock = DockStyle.Top; tbSpeed.Height = 50;
            tbSpeed.Minimum = 0; tbSpeed.Maximum = 100; tbSpeed.TickFrequency = 10;
            tbSpeed.ValueChanged += (s, e) => lblSpeed.Text = $"{tbSpeed.Value}%";

            lblSpeed.Text = "0%";
            lblSpeed.Font = new Font("Segoe UI", 15F, FontStyle.Bold);
            lblSpeed.ForeColor = Color.WhiteSmoke;
            lblSpeed.Dock = DockStyle.Top;
            lblSpeed.Height = 30;
            lblSpeed.TextAlign = ContentAlignment.MiddleCenter;

            // === ТАБЛИЦЯ ГРАФІКУ — БІЛИЙ ФОН + ЗЕЛЕНІ ВСТАНОВЛЕНІ ===
            var pnlSchedule = new Panel { Dock = DockStyle.Top, Height = 300, BackColor = Color.FromArgb(34, 34, 34) };
            var lblTitle = new Label { Text = "ГРАФІК РОБОТИ (хвилин / швидкість %)", Location = new Point(5, 5), ForeColor = Color.WhiteSmoke, Font = new Font("Segoe UI", 11F, FontStyle.Bold), AutoSize = true };

            dgvSchedule.Location = new Point(10, 27);
            dgvSchedule.Size = new Size(410, 210);
            dgvSchedule.BackgroundColor = Color.FromArgb(34,34,34);                    // білий фон
            dgvSchedule.GridColor = Color.DarkRed;                          // чорні лінії
            dgvSchedule.DefaultCellStyle.BackColor = Color.White;
            dgvSchedule.DefaultCellStyle.ForeColor = Color.Black;
            dgvSchedule.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(206, 145, 120);
            dgvSchedule.ColumnHeadersDefaultCellStyle.ForeColor = Color.Black;
            dgvSchedule.EnableHeadersVisualStyles = false;
            dgvSchedule.Columns.Add("min", "Хвилин");
            dgvSchedule.Columns.Add("speed", "Швидкість %");
            dgvSchedule.Columns[0].Width = 170;
            dgvSchedule.Columns[1].Width = 180;
            dgvSchedule.AllowUserToAddRows = false;

            // Приклад графіку
            dgvSchedule.Rows.Add("1", "10");
            dgvSchedule.Rows.Add("0", "0");
            dgvSchedule.Rows.Add("0", "0");
            dgvSchedule.Rows.Add("0", "0");
            dgvSchedule.Rows.Add("0", "0");
            dgvSchedule.Rows.Add("0", "0");
            dgvSchedule.Rows.Add("0", "0");
            dgvSchedule.Rows.Add("0", "0");

            btnLaunch.Text = "ЗАПУСК ПО ГРАФІКУ";
            btnLaunch.Location = new Point(10, 245);
            btnLaunch.Size = new Size(680, 50);                          // зменшено
            btnLaunch.BackColor = Color.Orange;
            btnLaunch.ForeColor = Color.Black;
            btnLaunch.Font = new Font("Segoe UI", 16F, FontStyle.Bold);
            btnLaunch.Click += StartSchedule;

            pnlSchedule.Controls.AddRange(new Control[] { lblTitle, dgvSchedule, btnLaunch });

            // === СТАТУС ===
            lblStatus.Text = "ГОТОВИЙ!";
            lblStatus.Font = new Font("Segoe UI", 15F, FontStyle.Bold);
            lblStatus.ForeColor = Color.Lime;
            lblStatus.Dock = DockStyle.Bottom;
            lblStatus.Height = 80;
            lblStatus.TextAlign = ContentAlignment.MiddleCenter;

            Controls.Add(lblStatus);
            Controls.Add(pnlSchedule);
            Controls.Add(lblSpeed);
            Controls.Add(tbSpeed);
            Controls.Add(btnEmergency);
            Controls.Add(pnlButtons);
            Controls.Add(pnlPort);
            Controls.Add(pbFlag);

            RefreshPorts();

            timerMain.Interval = 1000;
            timerMain.Tick += MainTimerTick;

            timerRead.Interval = 100;
            timerRead.Tick += (s, e) => { if (sp?.IsOpen == true && sp.BytesToRead > 0) lblStatus.Text = "← " + sp.ReadExisting().Trim(); };
            timerRead.Start();
        }

        private Bitmap CreateSmallFlag()
        {
            var bmp = new Bitmap(720, 70);
            using (var g = Graphics.FromImage(bmp))
            {
                g.FillRectangle(Brushes.RoyalBlue, 0, 0, 720, 35);
                g.FillRectangle(Brushes.Yellow, 0, 35, 720, 35);
                g.DrawString("СЛАВА УКРАЇНІ!", new Font("Arial Black", 16F), Brushes.Yellow, 260, 5);
                g.DrawString("Ромині котики нявкають!", new Font("Arial Black", 16F), Brushes.Blue, 200, 35);
            }
            return bmp;
        }

        private void RefreshPorts()
        {
            var sel = cmbPort.SelectedItem?.ToString();
            cmbPort.Items.Clear();
            foreach (var p in SerialPort.GetPortNames()) cmbPort.Items.Add(p);
            if (cmbPort.Items.Count > 0)
            {
                if (sel != null && cmbPort.Items.Contains(sel)) cmbPort.SelectedItem = sel;
                else cmbPort.SelectedIndex = 0;
            }
        }

        private void ToggleConnect(Button btn)
        {
            if (sp?.IsOpen == true)
            {
                sp.Close(); sp = null;
                btn.Text = "ПІДКЛЮЧИТИ"; btn.BackColor = Color.FromArgb(0, 180, 0);
                return;
            }
            if (cmbPort.SelectedItem == null) return;
            try
            {
                sp = new SerialPort(cmbPort.SelectedItem.ToString(), 115200) { NewLine = "\r\n", DtrEnable = true, RtsEnable = true };
                sp.Open();
                btn.Text = "ПІДКЛЮЧЕНО"; btn.BackColor = Color.Lime;
                lblStatus.Text = $"ПІДКЛЮЧЕНО • {cmbPort.SelectedItem}";
            }
            catch { MessageBox.Show("Помилка підключення"); }
        }

        private void Send(string cmd)
        {
            if (sp?.IsOpen == true)
            {
                sp.Write(cmd + "\r\n");
                lblSpeed.Text = cmd.Replace("SPEED:", "") + "%";
            }
        }

        private void StartSchedule(object s, EventArgs e)
        {
            StopAll();
            schedule.Clear();
            foreach (DataGridViewRow row in dgvSchedule.Rows)
            {
                if (row.IsNewRow) continue;
                if (int.TryParse(row.Cells[0].Value?.ToString(), out int min) &&
                    int.TryParse(row.Cells[1].Value?.ToString(), out int speed))
                {
                    if (min > 0) schedule.Add(new ScheduleItem { Minutes = min, Speed = speed });
                }
            }

            if (schedule.Count == 0) return;

            currentStep = 0;
            remainingSeconds = schedule[0].Minutes * 60;
            Send($"SPEED:{schedule[0].Speed}");
            lblStatus.Text = $"Крок {currentStep + 1}/{schedule.Count} • {schedule[0].Minutes} хв • {schedule[0].Speed}%";
            timerMain.Start();
        }

        private void MainTimerTick(object s, EventArgs e)
        {
            remainingSeconds--;
            int min = remainingSeconds / 60;
            int sec = remainingSeconds % 60;
            lblStatus.Text = $"Крок {currentStep + 1}/{schedule.Count} • {min:00}:{sec:00} • {schedule[currentStep].Speed}%";

            if (remainingSeconds <= 0)
            {
                currentStep++;
                if (currentStep >= schedule.Count)
                {
                    timerMain.Stop();
                    Send("SPEED:0");
                    lblStatus.Text = "ГРАФІК ЗАВЕРШЕНО!";
                    System.Media.SystemSounds.Asterisk.Play();
                    return;
                }
                remainingSeconds = schedule[currentStep].Minutes * 60;
                Send($"SPEED:{schedule[currentStep].Speed}");
                lblStatus.Text = $"Крок {currentStep + 1}/{schedule.Count} • {schedule[currentStep].Minutes} хв • {schedule[currentStep].Speed}%";
            }
        }

        private void StopAll()
        {
            timerMain.Stop();
            currentStep = 0;
        }

        private void FlashRed()
        {
            BackColor = Color.DarkRed;
            var t = new System.Windows.Forms.Timer { Interval = 300 };
            t.Tick += (se, ev) => { BackColor = Color.FromArgb(18, 18, 28); t.Stop(); t.Dispose(); };
            t.Start();
        }

        private class ScheduleItem
        {
            public int Minutes { get; set; }
            public int Speed { get; set; }
        }
    }

    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new CompactForm());
        }
    }
}