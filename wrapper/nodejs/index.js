/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 */

const { BqLog } = require('./build/Release/bqlog');

// Log levels enum
const LogLevel = {
    VERBOSE: 0,
    DEBUG: 1,
    INFO: 2,
    WARNING: 3,
    ERROR: 4,
    FATAL: 5
};

// Enhanced wrapper class with convenience methods
class BqLogJS extends BqLog {
    constructor(logId) {
        super(logId);
        this.logLevel = LogLevel;
    }

    // Convenience logging methods
    verbose(categoryIdx, message, ...args) {
        const formattedMessage = this._formatMessage(message, args);
        this.log(categoryIdx, LogLevel.VERBOSE, formattedMessage);
    }

    debug(categoryIdx, message, ...args) {
        const formattedMessage = this._formatMessage(message, args);
        this.log(categoryIdx, LogLevel.DEBUG, formattedMessage);
    }

    info(categoryIdx, message, ...args) {
        const formattedMessage = this._formatMessage(message, args);
        this.log(categoryIdx, LogLevel.INFO, formattedMessage);
    }

    warning(categoryIdx, message, ...args) {
        const formattedMessage = this._formatMessage(message, args);
        this.log(categoryIdx, LogLevel.WARNING, formattedMessage);
    }

    error(categoryIdx, message, ...args) {
        const formattedMessage = this._formatMessage(message, args);
        this.log(categoryIdx, LogLevel.ERROR, formattedMessage);
    }

    fatal(categoryIdx, message, ...args) {
        const formattedMessage = this._formatMessage(message, args);
        this.log(categoryIdx, LogLevel.FATAL, formattedMessage);
    }

    // Simple message formatting (similar to console.log)
    _formatMessage(message, args) {
        if (args.length === 0) {
            return String(message);
        }
        
        let formatted = String(message);
        args.forEach((arg, index) => {
            const placeholder = `{${index}}`;
            if (formatted.includes(placeholder)) {
                formatted = formatted.replace(placeholder, String(arg));
            } else {
                formatted += ' ' + String(arg);
            }
        });
        
        return formatted;
    }

    // Static factory methods
    static createLog(logName, config, categories = []) {
        return BqLog.createLog(logName, config, categories);
    }

    static getLogByName(logName) {
        return BqLog.getLogByName(logName);
    }

    static getVersion() {
        return BqLog.getVersion();
    }

    static enableAutoCrashHandler() {
        return BqLog.enableAutoCrashHandler();
    }

    static uninit() {
        return BqLog.uninit();
    }
}

// Export both the enhanced wrapper and original binding
module.exports = {
    BqLog: BqLogJS,
    BqLogNative: BqLog,
    LogLevel
};