#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_FIFO_PATH "/tmp/fifo_logger.fifo"
#define DEFAULT_LOG_PATH "/tmp/fifo_logger.log"
#define DEFAULT_ALARM_SECONDS 5
#define READ_BUFFER_SIZE 512

typedef struct Stats {
  unsigned long long messages;
  unsigned long long bytes;
  unsigned long long alarms;
} Stats;

static volatile sig_atomic_t terminate_requested = 0;
static volatile sig_atomic_t terminate_now = 0;
static volatile sig_atomic_t alarm_requested = 0;
static volatile sig_atomic_t stats_requested = 0;
static volatile sig_atomic_t daemonize_requested = 0;

static const char *program_name = "fifo_logger";
static const char *fifo_path = DEFAULT_FIFO_PATH;
static const char *log_path = DEFAULT_LOG_PATH;
static unsigned int alarm_seconds = DEFAULT_ALARM_SECONDS;

static FILE *log_stream = NULL;
static int foreground_mode = 1;
static int created_fifo = 0;
static Stats stats = {0, 0, 0};

static void
signal_handler(int signo)
{
  if (signo == SIGINT) {
    terminate_requested = SIGINT;
    terminate_now = 0;
  } else if (signo == SIGTERM) {
    terminate_requested = SIGTERM;
    terminate_now = 1;
  } else if (signo == SIGALRM) {
    alarm_requested = 1;
  } else if (signo == SIGUSR1) {
    stats_requested = 1;
  } else if (signo == SIGHUP) {
    daemonize_requested = 1;
  }
}

static void
log_printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(log_stream, fmt, ap);
  va_end(ap);
  fflush(log_stream);
}

static void
fatal(const char *fmt, ...)
{
  va_list ap;

  fprintf(stderr, "%s: ", program_name);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  exit(1);
}

static void
fatal_msg(const char *msg)
{
  fprintf(stderr, "%s: %s\n", program_name, msg);
  exit(1);
}

static int
parse_int(const char *s, const char *what)
{
  char *end;
  long value;

  errno = 0;
  value = strtol(s, &end, 10);
  if (errno != 0 || *s == '\0' || *end != '\0' || value <= 0 || value > INT_MAX) {
    fprintf(stderr, "%s: bad %s: %s\n", program_name, what, s);
    exit(1);
  }

  return (int)value;
}

static void
print_usage(FILE *out)
{
  fprintf(out,
    "Usage: %s [-d] [-f fifo_path] [-l log_path] [-n alarm_seconds]\n",
    program_name);
}

static void
parse_args(int argc, char *argv[])
{
  int opt;

  while ((opt = getopt(argc, argv, "df:l:n:h")) != -1) {
    switch (opt) {
      case 'd':
        foreground_mode = 0;
        break;
      case 'f':
        fifo_path = optarg;
        break;
      case 'l':
        log_path = optarg;
        break;
      case 'n':
        alarm_seconds = (unsigned int)parse_int(optarg, "alarm interval");
        break;
      case 'h':
        print_usage(stdout);
        exit(0);
      default:
        print_usage(stderr);
        exit(1);
    }
  }

  if (optind != argc) {
    print_usage(stderr);
    exit(1);
  }
}

static void
ensure_fifo(void)
{
  struct stat st;

  if (mkfifo(fifo_path, 0600) == 0) {
    created_fifo = 1;
    return;
  }

  if (errno != EEXIST)
    fatal("mkfifo(%s)", fifo_path);

  if (stat(fifo_path, &st) < 0)
    fatal("stat(%s)", fifo_path);

  if (!S_ISFIFO(st.st_mode))
    fatal_msg("existing path is not a FIFO");
}

static void
close_log_stream(void)
{
  if (log_stream != NULL && log_stream != stdout)
    fclose(log_stream);
  log_stream = NULL;
}

static void
open_log_stream_or_die(void)
{
  if (foreground_mode) {
    log_stream = stdout;
    return;
  }

  log_stream = fopen(log_path, "a");
  if (log_stream == NULL)
    fatal("fopen(%s)", log_path);
}

static void
install_signal(int signo, void (*handler)(int))
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(signo, &sa, NULL) < 0)
    fatal("sigaction(%d)", signo);
}

static void
install_signal_handlers(void)
{
  install_signal(SIGINT, signal_handler);
  install_signal(SIGTERM, signal_handler);
  install_signal(SIGALRM, signal_handler);
  install_signal(SIGUSR1, signal_handler);
  install_signal(SIGHUP, signal_handler);
  install_signal(SIGQUIT, SIG_IGN);
}

static void
write_statistics(const char *reason)
{
  log_printf(
    "statistics (%s): messages=%llu bytes=%llu alarms=%llu\n",
    reason,
    stats.messages,
    stats.bytes,
    stats.alarms);
}

static void
handle_alarm(void)
{
  if (!alarm_requested)
    return;

  stats.alarms++;
  alarm_requested = 0;
  log_printf("waiting for data on %s\n", fifo_path);
  alarm(alarm_seconds);
}

static void
handle_stats_request(void)
{
  if (!stats_requested)
    return;

  stats_requested = 0;
  write_statistics("SIGUSR1");
}

static void
redirect_stdio_to_log(void)
{
  FILE *new_log;
  int fd;

  fd = open("/dev/null", O_RDONLY);
  if (fd < 0)
    fatal("open(/dev/null)");
  if (dup2(fd, STDIN_FILENO) < 0)
    fatal("dup2(stdin)");
  if (fd > STDERR_FILENO)
    close(fd);

  new_log = fopen(log_path, "a");
  if (new_log == NULL)
    fatal("fopen(%s)", log_path);

  if (dup2(fileno(new_log), STDOUT_FILENO) < 0)
    fatal("dup2(stdout)");
  if (dup2(fileno(new_log), STDERR_FILENO) < 0)
    fatal("dup2(stderr)");
  fclose(new_log);

  close_log_stream();
  log_stream = stdout;
  if (setvbuf(log_stream, NULL, _IOLBF, 0) != 0)
    fatal("setvbuf");
}

static void
daemonize_process(void)
{
  pid_t pid;

  pid = fork();
  if (pid < 0)
    fatal("fork");
  if (pid > 0)
    exit(0);

  if (setsid() < 0)
    fatal("setsid");

  pid = fork();
  if (pid < 0)
    fatal("fork");
  if (pid > 0)
    exit(0);

  if (chdir("/") < 0)
    fatal("chdir(/)");

  redirect_stdio_to_log();
  foreground_mode = 0;
}

static void
become_daemon(void)
{
  if (!foreground_mode) {
    daemonize_requested = 0;
    return;
  }

  daemonize_requested = 0;
  write_statistics("before SIGHUP daemonize");
  daemonize_process();
  log_printf("daemonized after SIGHUP\n");
  write_statistics("after SIGHUP daemonize");
}

static void
handle_signal_actions(void)
{
  if (terminate_now)
    return;

  handle_alarm();
  handle_stats_request();
  if (daemonize_requested && !terminate_requested)
    become_daemon();
}

static void
cleanup(void)
{
  if (created_fifo)
    unlink(fifo_path);
  close_log_stream();
}

static void
finish_and_exit(int code, const char *reason)
{
  write_statistics(reason);
  cleanup();
  exit(code);
}

static int
open_fifo_for_read(void)
{
  int fd;

  while (1) {
    if (terminate_requested)
      return -1;

    handle_signal_actions();
    if (terminate_requested)
      return -1;

    fd = open(fifo_path, O_RDONLY);
    if (fd >= 0)
      return fd;

    if (errno == EINTR) {
      if (terminate_requested)
        return -1;
      handle_signal_actions();
      continue;
    }

    fatal("open(%s)", fifo_path);
  }
}

static ssize_t
read_fifo_retry(int fd, char *buf, size_t size)
{
  ssize_t nread;

  while (1) {
    if (terminate_now)
      return -2;

    handle_signal_actions();
    if (terminate_now)
      return -2;

    nread = read(fd, buf, size);
    if (nread >= 0)
      return nread;

    if (errno == EINTR) {
      if (terminate_now)
        return -2;
      handle_signal_actions();
      if (terminate_now)
        return -2;
      continue;
    }

    fatal("read(%s)", fifo_path);
  }
}

int
main(int argc, char *argv[])
{
  int fd;

  program_name = argv[0];
  parse_args(argc, argv);
  if (foreground_mode) {
    open_log_stream_or_die();
    if (setvbuf(log_stream, NULL, _IOLBF, 0) != 0)
      fatal("setvbuf");
  } else {
    daemonize_process();
  }

  ensure_fifo();
  install_signal_handlers();
  alarm(alarm_seconds);

  log_printf("started: fifo=%s mode=%s alarm=%u\n",
    fifo_path,
    foreground_mode ? "foreground" : "daemon",
    alarm_seconds);

  while (!terminate_requested) {
    char buf[READ_BUFFER_SIZE + 1];
    ssize_t nread;
    int saw_data = 0;
    int last_char = '\n';

    handle_signal_actions();

    fd = open_fifo_for_read();
    if (fd < 0)
      break;

    while (1) {
      nread = read_fifo_retry(fd, buf, READ_BUFFER_SIZE);
      if (nread == -2)
        break;

      handle_signal_actions();
      if (terminate_now)
        break;

      if (nread == 0)
        break;

      buf[nread] = '\0';
      log_printf("%s", buf);

      saw_data = 1;
      stats.bytes += (unsigned long long)nread;
      last_char = (unsigned char)buf[nread - 1];

      handle_signal_actions();
      if (terminate_now)
        break;
    }

    close(fd);
    handle_signal_actions();

    if (saw_data) {
      if (last_char != '\n')
        log_printf("\n");
      stats.messages++;
    }

    if (terminate_now)
      break;

    if (terminate_requested == SIGINT)
      break;
  }

  if (terminate_requested == SIGINT)
    log_printf("received SIGINT, current message drained\n");
  else if (terminate_requested == SIGTERM)
    log_printf("received SIGTERM, stopping immediately\n");

  finish_and_exit(0, "shutdown");
}
