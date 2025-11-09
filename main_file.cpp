#include <unistd.h>
#include <dirent.h>
#include <cctype>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <ncurses.h>

using std::string;
using std::vector;
using std::to_string;

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
const string kProcDirectory{"/proc/"};
const string kVersionFilename{"/version"};
const string kMeminfoFilename{"/meminfo"};
const string kUptimeFilename{"/uptime"};
const string kStatFilename{"/stat"};
const string kCmdlineFilename{"/cmdline"};
const string kStatusFilename{"/status"};
const string kOSPath{"/etc/os-release"};
const string kPasswordPath{"/etc/passwd"};

// -----------------------------------------------------------------------------
// Helper: Format elapsed time
// -----------------------------------------------------------------------------
string ElapsedTime(long seconds) {
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  int s = seconds % 60;
  std::ostringstream out;
  out << std::setw(2) << std::setfill('0') << h << ":"
      << std::setw(2) << std::setfill('0') << m << ":"
      << std::setw(2) << std::setfill('0') << s;
  return out.str();
}

// -----------------------------------------------------------------------------
// LinuxParser namespace
// -----------------------------------------------------------------------------
namespace LinuxParser {
string KeyValParser(string key, string path) {
  string value = "n/a", temp, line;
  std::ifstream stream(path);
  if (stream.is_open()) {
    while (std::getline(stream, line)) {
      std::istringstream linestream(line);
      linestream >> temp;
      if (temp == key) {
        linestream >> value;
        break;
      }
    }
  }
  return value;
}

string OperatingSystem() {
  string line, key, value = "n/a";
  std::ifstream filestream(kOSPath);
  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::replace(line.begin(), line.end(), ' ', '_');
      std::replace(line.begin(), line.end(), '=', ' ');
      std::replace(line.begin(), line.end(), '"', ' ');
      std::istringstream linestream(line);
      while (linestream >> key >> value) {
        if (key == "PRETTY_NAME") {
          std::replace(value.begin(), value.end(), '_', ' ');
          return value;
        }
      }
    }
  }
  return value;
}

string Kernel() {
  string os, version, kernel;
  std::ifstream stream(kProcDirectory + kVersionFilename);
  if (stream.is_open()) {
    stream >> os >> version >> kernel;
  }
  return kernel;
}

vector<int> Pids() {
  vector<int> pids;
  DIR* directory = opendir(kProcDirectory.c_str());
  struct dirent* file;
  while ((file = readdir(directory)) != nullptr) {
    if (file->d_type == DT_DIR) {
      string filename(file->d_name);
      if (std::all_of(filename.begin(), filename.end(), isdigit)) {
        pids.push_back(stoi(filename));
      }
    }
  }
  closedir(directory);
  return pids;
}

vector<string> CpuUtilization() {
  vector<string> values;
  string line, key, val;
  std::ifstream stream(kProcDirectory + kStatFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    linestream >> key;
    for (int i = 0; i < 10; ++i) {
      linestream >> val;
      values.push_back(val);
    }
  }
  return values;
}

float MemoryUtilization() {
  string memTotalStr = KeyValParser("MemTotal:", kProcDirectory + kMeminfoFilename);
  string memFreeStr = KeyValParser("MemFree:", kProcDirectory + kMeminfoFilename);
  float memTotal = std::stof(memTotalStr);
  float memFree = std::stof(memFreeStr);
  return (memTotal - memFree) / memTotal;
}

long UpTime() {
  long uptime = 0;
  std::ifstream stream(kProcDirectory + kUptimeFilename);
  if (stream.is_open()) {
    stream >> uptime;
  }
  return uptime;
}

int TotalProcesses() {
  return stoi(KeyValParser("processes", kProcDirectory + kStatFilename));
}

int RunningProcesses() {
  return stoi(KeyValParser("procs_running", kProcDirectory + kStatFilename));
}

long Jiffies() {
  auto cpu = CpuUtilization();
  long total = 0;
  for (auto& s : cpu) total += stol(s);
  return total;
}

long ActiveJiffies() {
  auto cpu = CpuUtilization();
  return stol(cpu[0]) + stol(cpu[1]) + stol(cpu[2]) + stol(cpu[5]) + stol(cpu[6]) + stol(cpu[7]);
}

long IdleJiffies() {
  auto cpu = CpuUtilization();
  return stol(cpu[3]) + stol(cpu[4]);
}

string Uid(int pid) {
  return KeyValParser("Uid:", kProcDirectory + to_string(pid) + kStatusFilename);
}

string User(int pid) {
  string uid = Uid(pid), line, user, x, id;
  std::ifstream stream(kPasswordPath);
  if (stream.is_open()) {
    while (std::getline(stream, line)) {
      std::replace(line.begin(), line.end(), ':', ' ');
      std::istringstream linestream(line);
      linestream >> user >> x >> id;
      if (id == uid) return user;
    }
  }
  return "n/a";
}

string Command(int pid) {
  string line;
  std::ifstream stream(kProcDirectory + to_string(pid) + kCmdlineFilename);
  if (stream.is_open()) std::getline(stream, line);
  return line;
}

string Ram(int pid) {
  return KeyValParser("VmSize:", kProcDirectory + to_string(pid) + kStatusFilename);
}

long ActiveJiffies(int pid) {
  string line, value;
  std::ifstream stream(kProcDirectory + to_string(pid) + kStatFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    vector<string> values;
    for (string s; linestream >> s;) values.push_back(s);
    if (values.size() > 16)
      return stol(values[13]) + stol(values[14]) + stol(values[15]) + stol(values[16]);
  }
  return 0;
}

long UpTime(int pid) {
  string line;
  std::ifstream stream(kProcDirectory + to_string(pid) + kStatFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    vector<string> values;
    for (string s; linestream >> s;) values.push_back(s);
    if (values.size() > 21)
      return UpTime() - stol(values[21]) / sysconf(_SC_CLK_TCK);
  }
  return 0;
}
}  // namespace LinuxParser

// -----------------------------------------------------------------------------
// Processor
// -----------------------------------------------------------------------------
class Processor {
 public:
  float Utilization() {
    long active = LinuxParser::ActiveJiffies();
    long total = LinuxParser::Jiffies();
    long deltaActive = active - prevActive_;
    long deltaTotal = total - prevTotal_;
    prevActive_ = active;
    prevTotal_ = total;
    return deltaTotal ? static_cast<float>(deltaActive) / deltaTotal : 0.0;
  }
 private:
  long prevActive_{0}, prevTotal_{0};
};

// -----------------------------------------------------------------------------
// Process
// -----------------------------------------------------------------------------
class Process {
 public:
  int Pid() const { return pid_; }
  string User() const { return user_; }
  string Command() const { return command_; }
  string Ram() const { return ram_; }
  long UpTime() const { return uptime_; }
  float CpuUtilization() const { return cpu_; }
  bool operator>(Process const& a) const { return cpu_ > a.cpu_; }

  void Pid(int pid) { pid_ = pid; }
  void User(int pid) { user_ = LinuxParser::User(pid); }
  void Command(int pid) { command_ = LinuxParser::Command(pid); }
  void Ram(int pid) {
    string val = LinuxParser::Ram(pid);
    ram_ = (val == "n/a" ? "0" : to_string(stol(val) / 1024));
  }
  void UpTime(int pid) { uptime_ = LinuxParser::UpTime(pid); }
  void CpuUtilization(int pid) {
    long total = LinuxParser::ActiveJiffies(pid);
    long seconds = LinuxParser::UpTime() - LinuxParser::UpTime(pid);
    cpu_ = seconds ? ((float)(total / sysconf(_SC_CLK_TCK)) / seconds) : 0.0;
  }

 private:
  int pid_;
  string user_, command_, ram_;
  long uptime_;
  float cpu_;
};

// -----------------------------------------------------------------------------
// System
// -----------------------------------------------------------------------------
class System {
 public:
  Processor& Cpu() { return cpu_; }
  vector<Process>& Processes() {
    vector<int> pids = LinuxParser::Pids();
    processes_.clear();
    for (int pid : pids) {
      Process p;
      p.Pid(pid);
      p.User(pid);
      p.Command(pid);
      p.CpuUtilization(pid);
      p.Ram(pid);
      p.UpTime(pid);
      processes_.push_back(p);
    }
    std::sort(processes_.begin(), processes_.end(), std::greater<Process>());
    if (processes_.size() > 50) processes_.resize(50);
    return processes_;
  }
  string Kernel() { return LinuxParser::Kernel(); }
  string OperatingSystem() { return LinuxParser::OperatingSystem(); }
  float MemoryUtilization() { return LinuxParser::MemoryUtilization(); }
  int RunningProcesses() { return LinuxParser::RunningProcesses(); }
  int TotalProcesses() { return LinuxParser::TotalProcesses(); }
  long UpTime() { return LinuxParser::UpTime(); }

 private:
  Processor cpu_;
  vector<Process> processes_;
};

// -----------------------------------------------------------------------------
// Ncurses Display
// -----------------------------------------------------------------------------
namespace NCursesDisplay {
string ProgressBar(float percent) {
  string bar;
  int size = 50;
  int filled = percent * size;
  for (int i = 0; i < size; ++i)
    bar += (i < filled ? '|' : ' ');
  std::ostringstream out;
  out << " " << std::setw(4) << std::fixed << std::setprecision(1) << percent * 100 << "%";
  return bar + out.str();
}

void DisplaySystem(System& system, WINDOW* window) {
  int row = 0;
  mvwprintw(window, ++row, 2, ("OS: " + system.OperatingSystem()).c_str());
  mvwprintw(window, ++row, 2, ("Kernel: " + system.Kernel()).c_str());
  mvwprintw(window, ++row, 2, "CPU: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, ProgressBar(system.Cpu().Utilization()).c_str());
  wattroff(window, COLOR_PAIR(1));
  mvwprintw(window, ++row, 2, "Memory: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, ProgressBar(system.MemoryUtilization()).c_str());
  wattroff(window, COLOR_PAIR(1));
  mvwprintw(window, ++row, 2, ("Total Processes: " + to_string(system.TotalProcesses())).c_str());
  mvwprintw(window, ++row, 2, ("Running: " + to_string(system.RunningProcesses())).c_str());
  mvwprintw(window, ++row, 2, ("Uptime: " + ElapsedTime(system.UpTime())).c_str());
  wrefresh(window);
}

void DisplayProcesses(vector<Process>& procs, WINDOW* window) {
  int row = 0;
  wattron(window, COLOR_PAIR(2));
  mvwprintw(window, ++row, 2, "PID      USER        CPU%%   RAM(MB)  TIME     COMMAND");
  wattroff(window, COLOR_PAIR(2));
  for (size_t i = 0; i < procs.size() && i < 20; ++i) {
    mvwprintw(window, ++row, 2, "%d", procs[i].Pid());
    mvwprintw(window, row, 11, "%s", procs[i].User().c_str());
    mvwprintw(window, row, 24, "%.1f", procs[i].CpuUtilization() * 100);
    mvwprintw(window, row, 33, "%s", procs[i].Ram().c_str());
    mvwprintw(window, row, 43, "%s", ElapsedTime(procs[i].UpTime()).c_str());
    mvwprintw(window, row, 55, "%s", procs[i].Command().substr(0, getmaxx(window) - 55).c_str());
  }
  wrefresh(window);
}

void Display(System& system) {
  initscr(); noecho(); cbreak(); start_color(); nodelay(stdscr, TRUE);
  curs_set(0); init_pair(1, COLOR_BLUE, COLOR_BLACK); init_pair(2, COLOR_GREEN, COLOR_BLACK);
  int x_max = getmaxx(stdscr);
  WINDOW* syswin = newwin(10, x_max - 1, 0, 0);
  WINDOW* procwin = newwin(25, x_max - 1, 11, 0);

  while (true) {
    werase(syswin); werase(procwin);
    box(syswin, 0, 0); box(procwin, 0, 0);
    DisplaySystem(system, syswin);
    DisplayProcesses(system.Processes(), procwin);
    wrefresh(syswin); wrefresh(procwin); refresh();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int ch = getch();
    if (ch == 'q' || ch == 'Q') break;
  }

  delwin(syswin); delwin(procwin); endwin();
}
}  // namespace NCursesDisplay

// -----------------------------------------------------------------------------
// main()
// -----------------------------------------------------------------------------
int main() {
  System system;
  NCursesDisplay::Display(system);
  return 0;
}
#include <unistd.h>
#include <dirent.h>
#include <cctype>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <ncurses.h>

using std::string;
using std::vector;
using std::to_string;

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
const string kProcDirectory{"/proc/"};
const string kVersionFilename{"/version"};
const string kMeminfoFilename{"/meminfo"};
const string kUptimeFilename{"/uptime"};
const string kStatFilename{"/stat"};
const string kCmdlineFilename{"/cmdline"};
const string kStatusFilename{"/status"};
const string kOSPath{"/etc/os-release"};
const string kPasswordPath{"/etc/passwd"};

// -----------------------------------------------------------------------------
// Helper: Format elapsed time
// -----------------------------------------------------------------------------
string ElapsedTime(long seconds) {
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  int s = seconds % 60;
  std::ostringstream out;
  out << std::setw(2) << std::setfill('0') << h << ":"
      << std::setw(2) << std::setfill('0') << m << ":"
      << std::setw(2) << std::setfill('0') << s;
  return out.str();
}

// -----------------------------------------------------------------------------
// LinuxParser namespace
// -----------------------------------------------------------------------------
namespace LinuxParser {
string KeyValParser(string key, string path) {
  string value = "n/a", temp, line;
  std::ifstream stream(path);
  if (stream.is_open()) {
    while (std::getline(stream, line)) {
      std::istringstream linestream(line);
      linestream >> temp;
      if (temp == key) {
        linestream >> value;
        break;
      }
    }
  }
  return value;
}

string OperatingSystem() {
  string line, key, value = "n/a";
  std::ifstream filestream(kOSPath);
  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::replace(line.begin(), line.end(), ' ', '_');
      std::replace(line.begin(), line.end(), '=', ' ');
      std::replace(line.begin(), line.end(), '"', ' ');
      std::istringstream linestream(line);
      while (linestream >> key >> value) {
        if (key == "PRETTY_NAME") {
          std::replace(value.begin(), value.end(), '_', ' ');
          return value;
        }
      }
    }
  }
  return value;
}

string Kernel() {
  string os, version, kernel;
  std::ifstream stream(kProcDirectory + kVersionFilename);
  if (stream.is_open()) {
    stream >> os >> version >> kernel;
  }
  return kernel;
}

vector<int> Pids() {
  vector<int> pids;
  DIR* directory = opendir(kProcDirectory.c_str());
  struct dirent* file;
  while ((file = readdir(directory)) != nullptr) {
    if (file->d_type == DT_DIR) {
      string filename(file->d_name);
      if (std::all_of(filename.begin(), filename.end(), isdigit)) {
        pids.push_back(stoi(filename));
      }
    }
  }
  closedir(directory);
  return pids;
}

vector<string> CpuUtilization() {
  vector<string> values;
  string line, key, val;
  std::ifstream stream(kProcDirectory + kStatFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    linestream >> key;
    for (int i = 0; i < 10; ++i) {
      linestream >> val;
      values.push_back(val);
    }
  }
  return values;
}

float MemoryUtilization() {
  string memTotalStr = KeyValParser("MemTotal:", kProcDirectory + kMeminfoFilename);
  string memFreeStr = KeyValParser("MemFree:", kProcDirectory + kMeminfoFilename);
  float memTotal = std::stof(memTotalStr);
  float memFree = std::stof(memFreeStr);
  return (memTotal - memFree) / memTotal;
}

long UpTime() {
  long uptime = 0;
  std::ifstream stream(kProcDirectory + kUptimeFilename);
  if (stream.is_open()) {
    stream >> uptime;
  }
  return uptime;
}

int TotalProcesses() {
  return stoi(KeyValParser("processes", kProcDirectory + kStatFilename));
}

int RunningProcesses() {
  return stoi(KeyValParser("procs_running", kProcDirectory + kStatFilename));
}

long Jiffies() {
  auto cpu = CpuUtilization();
  long total = 0;
  for (auto& s : cpu) total += stol(s);
  return total;
}

long ActiveJiffies() {
  auto cpu = CpuUtilization();
  return stol(cpu[0]) + stol(cpu[1]) + stol(cpu[2]) + stol(cpu[5]) + stol(cpu[6]) + stol(cpu[7]);
}

long IdleJiffies() {
  auto cpu = CpuUtilization();
  return stol(cpu[3]) + stol(cpu[4]);
}

string Uid(int pid) {
  return KeyValParser("Uid:", kProcDirectory + to_string(pid) + kStatusFilename);
}

string User(int pid) {
  string uid = Uid(pid), line, user, x, id;
  std::ifstream stream(kPasswordPath);
  if (stream.is_open()) {
    while (std::getline(stream, line)) {
      std::replace(line.begin(), line.end(), ':', ' ');
      std::istringstream linestream(line);
      linestream >> user >> x >> id;
      if (id == uid) return user;
    }
  }
  return "n/a";
}

string Command(int pid) {
  string line;
  std::ifstream stream(kProcDirectory + to_string(pid) + kCmdlineFilename);
  if (stream.is_open()) std::getline(stream, line);
  return line;
}

string Ram(int pid) {
  return KeyValParser("VmSize:", kProcDirectory + to_string(pid) + kStatusFilename);
}

long ActiveJiffies(int pid) {
  string line, value;
  std::ifstream stream(kProcDirectory + to_string(pid) + kStatFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    vector<string> values;
    for (string s; linestream >> s;) values.push_back(s);
    if (values.size() > 16)
      return stol(values[13]) + stol(values[14]) + stol(values[15]) + stol(values[16]);
  }
  return 0;
}

long UpTime(int pid) {
  string line;
  std::ifstream stream(kProcDirectory + to_string(pid) + kStatFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    vector<string> values;
    for (string s; linestream >> s;) values.push_back(s);
    if (values.size() > 21)
      return UpTime() - stol(values[21]) / sysconf(_SC_CLK_TCK);
  }
  return 0;
}
}  // namespace LinuxParser

// -----------------------------------------------------------------------------
// Processor
// -----------------------------------------------------------------------------
class Processor {
 public:
  float Utilization() {
    long active = LinuxParser::ActiveJiffies();
    long total = LinuxParser::Jiffies();
    long deltaActive = active - prevActive_;
    long deltaTotal = total - prevTotal_;
    prevActive_ = active;
    prevTotal_ = total;
    return deltaTotal ? static_cast<float>(deltaActive) / deltaTotal : 0.0;
  }
 private:
  long prevActive_{0}, prevTotal_{0};
};

// -----------------------------------------------------------------------------
// Process
// -----------------------------------------------------------------------------
class Process {
 public:
  int Pid() const { return pid_; }
  string User() const { return user_; }
  string Command() const { return command_; }
  string Ram() const { return ram_; }
  long UpTime() const { return uptime_; }
  float CpuUtilization() const { return cpu_; }
  bool operator>(Process const& a) const { return cpu_ > a.cpu_; }

  void Pid(int pid) { pid_ = pid; }
  void User(int pid) { user_ = LinuxParser::User(pid); }
  void Command(int pid) { command_ = LinuxParser::Command(pid); }
  void Ram(int pid) {
    string val = LinuxParser::Ram(pid);
    ram_ = (val == "n/a" ? "0" : to_string(stol(val) / 1024));
  }
  void UpTime(int pid) { uptime_ = LinuxParser::UpTime(pid); }
  void CpuUtilization(int pid) {
    long total = LinuxParser::ActiveJiffies(pid);
    long seconds = LinuxParser::UpTime() - LinuxParser::UpTime(pid);
    cpu_ = seconds ? ((float)(total / sysconf(_SC_CLK_TCK)) / seconds) : 0.0;
  }

 private:
  int pid_;
  string user_, command_, ram_;
  long uptime_;
  float cpu_;
};

// -----------------------------------------------------------------------------
// System
// -----------------------------------------------------------------------------
class System {
 public:
  Processor& Cpu() { return cpu_; }
  vector<Process>& Processes() {
    vector<int> pids = LinuxParser::Pids();
    processes_.clear();
    for (int pid : pids) {
      Process p;
      p.Pid(pid);
      p.User(pid);
      p.Command(pid);
      p.CpuUtilization(pid);
      p.Ram(pid);
      p.UpTime(pid);
      processes_.push_back(p);
    }
    std::sort(processes_.begin(), processes_.end(), std::greater<Process>());
    if (processes_.size() > 50) processes_.resize(50);
    return processes_;
  }
  string Kernel() { return LinuxParser::Kernel(); }
  string OperatingSystem() { return LinuxParser::OperatingSystem(); }
  float MemoryUtilization() { return LinuxParser::MemoryUtilization(); }
  int RunningProcesses() { return LinuxParser::RunningProcesses(); }
  int TotalProcesses() { return LinuxParser::TotalProcesses(); }
  long UpTime() { return LinuxParser::UpTime(); }

 private:
  Processor cpu_;
  vector<Process> processes_;
};

// -----------------------------------------------------------------------------
// Ncurses Display
// -----------------------------------------------------------------------------
namespace NCursesDisplay {
string ProgressBar(float percent) {
  string bar;
  int size = 50;
  int filled = percent * size;
  for (int i = 0; i < size; ++i)
    bar += (i < filled ? '|' : ' ');
  std::ostringstream out;
  out << " " << std::setw(4) << std::fixed << std::setprecision(1) << percent * 100 << "%";
  return bar + out.str();
}

void DisplaySystem(System& system, WINDOW* window) {
  int row = 0;
  mvwprintw(window, ++row, 2, ("OS: " + system.OperatingSystem()).c_str());
  mvwprintw(window, ++row, 2, ("Kernel: " + system.Kernel()).c_str());
  mvwprintw(window, ++row, 2, "CPU: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, ProgressBar(system.Cpu().Utilization()).c_str());
  wattroff(window, COLOR_PAIR(1));
  mvwprintw(window, ++row, 2, "Memory: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, ProgressBar(system.MemoryUtilization()).c_str());
  wattroff(window, COLOR_PAIR(1));
  mvwprintw(window, ++row, 2, ("Total Processes: " + to_string(system.TotalProcesses())).c_str());
  mvwprintw(window, ++row, 2, ("Running: " + to_string(system.RunningProcesses())).c_str());
  mvwprintw(window, ++row, 2, ("Uptime: " + ElapsedTime(system.UpTime())).c_str());
  wrefresh(window);
}

void DisplayProcesses(vector<Process>& procs, WINDOW* window) {
  int row = 0;
  wattron(window, COLOR_PAIR(2));
  mvwprintw(window, ++row, 2, "PID      USER        CPU%%   RAM(MB)  TIME     COMMAND");
  wattroff(window, COLOR_PAIR(2));
  for (size_t i = 0; i < procs.size() && i < 20; ++i) {
    mvwprintw(window, ++row, 2, "%d", procs[i].Pid());
    mvwprintw(window, row, 11, "%s", procs[i].User().c_str());
    mvwprintw(window, row, 24, "%.1f", procs[i].CpuUtilization() * 100);
    mvwprintw(window, row, 33, "%s", procs[i].Ram().c_str());
    mvwprintw(window, row, 43, "%s", ElapsedTime(procs[i].UpTime()).c_str());
    mvwprintw(window, row, 55, "%s", procs[i].Command().substr(0, getmaxx(window) - 55).c_str());
  }
  wrefresh(window);
}

void Display(System& system) {
  initscr(); noecho(); cbreak(); start_color(); nodelay(stdscr, TRUE);
  curs_set(0); init_pair(1, COLOR_BLUE, COLOR_BLACK); init_pair(2, COLOR_GREEN, COLOR_BLACK);
  int x_max = getmaxx(stdscr);
  WINDOW* syswin = newwin(10, x_max - 1, 0, 0);
  WINDOW* procwin = newwin(25, x_max - 1, 11, 0);

  while (true) {
    werase(syswin); werase(procwin);
    box(syswin, 0, 0); box(procwin, 0, 0);
    DisplaySystem(system, syswin);
    DisplayProcesses(system.Processes(), procwin);
    wrefresh(syswin); wrefresh(procwin); refresh();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int ch = getch();
    if (ch == 'q' || ch == 'Q') break;
  }

  delwin(syswin); delwin(procwin); endwin();
}
}  // namespace NCursesDisplay

// -----------------------------------------------------------------------------
// main()
// -----------------------------------------------------------------------------
int main() {
  System system;
  NCursesDisplay::Display(system);
  return 0;
}
