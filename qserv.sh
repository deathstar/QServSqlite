#!/bin/bash

# Automatically set BASE_DIR to the directory where this script is located
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Configuration
APP_NAME="qserv"
PID_FILE="$BASE_DIR/qserv.pid"
LOG_FILE="$BASE_DIR/qserv.log"

# Function to get the PID if running
get_pid() {
    [ -f "$PID_FILE" ] && cat "$PID_FILE"
}

case "$1" in
    start)
        if [ -f "$PID_FILE" ] && ps -p $(cat "$PID_FILE") > /dev/null; then
            echo "Qserv is already running."
        else
            echo "Starting Qserv from $BASE_DIR..."
            cd "$BASE_DIR" && nohup ./$APP_NAME > "$LOG_FILE" 2>&1 < /dev/null &
            echo $! > "$PID_FILE"
            echo "Qserv started (PID: $(cat $PID_FILE)). Logs: $LOG_FILE"
        fi
        ;;
    stop)
        PID=$(get_pid)
        if [ -z "$PID" ]; then
            echo "Qserv is not running."
        else
            echo "Stopping Qserv (PID: $PID)..."
            kill "$PID" && rm -f "$PID_FILE"
            echo "Stopped."
        fi
        ;;
    status)
        PID=$(get_pid)
        if [ -n "$PID" ] && ps -p "$PID" > /dev/null; then
            echo "Qserv is running (PID: $PID)."
        else
            echo "Qserv is not running."
            [ -f "$PID_FILE" ] && rm -f "$PID_FILE"
        fi
        ;;
    log)
        if [ -f "$LOG_FILE" ]; then
            tail -f "$LOG_FILE"
        else
            echo "Log file not found."
        fi
        ;;
    monitor)
        PID=$(get_pid)
        if [ -z "$PID" ] || ! ps -p "$PID" > /dev/null; then
            echo "$(date): Qserv down. Restarting..."
            $0 start
        fi
        ;;
    *)
        echo "Usage: ./qserv.sh {start|stop|status|log|monitor}"
        echo ""
        echo "Simple manager for Qserv."
        echo "Commands:"
        echo "  start    - Starts the service in the background."
        echo "  stop     - Stops the service gracefully."
        echo "  status   - Checks if the service is active."
        echo "  log      - Streams the log file to the terminal."
        echo "  monitor  - Use this in crontab for auto-restart."
        exit 1
        ;;
esac
