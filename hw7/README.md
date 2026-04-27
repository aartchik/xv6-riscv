# FIFO Log Server

## Files

- `fifo_logger.c` - решение задания на POSIX C.
- `Makefile` - сборка через `make`.

## Build

```sh
make
```

## Run

Foreground:

```sh
./fifo_logger
```

Daemon mode:

```sh
./fifo_logger -d
```

Custom paths and alarm interval:

```sh
./fifo_logger -f /tmp/my_fifo -l /tmp/my_log.log -n 3
```

## Signals

- `SIGINT` - дочитать уже открытый FIFO до `EOF`, вывести статистику и завершиться.
- `SIGTERM` - завершиться сразу, не дочитывая текущий канал.
- `SIGQUIT` - игнорируется.
- `SIGUSR1` - вывести статистику без завершения.
- `SIGHUP` - если процесс в foreground, демонизироваться и продолжить писать лог в файл.

## Quick Checks

```sh
printf 'hello\nworld' > /tmp/fifo_logger.fifo
kill -USR1 <pid>
kill -HUP <pid>
kill -TERM <pid>
```
