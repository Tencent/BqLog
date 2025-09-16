/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * 
 * HarmonyOS Node.js Native Module Example
 * This example demonstrates how to use BqLog in HarmonyOS applications
 * using the same Node API binding as regular Node.js
 */

const { BqLog, LogLevel } = require('../index');

// HarmonyOS optimized configuration
const harmonyOSConfig = JSON.stringify({
    "appenders_config": [
        {
            "type": "console",
            "name": "harmony_console",
            "levels": ["info", "warning", "error", "fatal"],
            "enable": true
        },
        {
            "type": "file",
            "name": "harmony_file",
            "file_name": "/data/storage/el2/base/files/logs/harmony_app.log",
            "levels": ["all"],
            "enable": true,
            "max_file_size": "10MB",
            "max_file_count": 5
        }
    ],
    "global_config": {
        "buffer_size": "2MB",
        "thread_mode": "async",
        "compress_mode": "zstd"
    }
});

class HarmonyOSLogger {
    constructor(appName) {
        this.appName = appName;
        this.categories = ['UI', 'Business', 'Network', 'Storage', 'System'];
        this.logger = null;
        this.init();
    }

    init() {
        console.log(`Initializing BqLog for HarmonyOS app: ${this.appName}`);
        console.log('BqLog Version:', BqLog.getVersion());
        
        // Enable crash handling for better stability
        BqLog.enableAutoCrashHandler();
        
        // Create logger instance
        this.logger = BqLog.createLog(this.appName, harmonyOSConfig, this.categories);
        
        if (!this.logger || !this.logger.isValid()) {
            throw new Error('Failed to initialize BqLog for HarmonyOS');
        }
        
        console.log('HarmonyOS Logger initialized successfully');
        this.logger.info(4, 'HarmonyOS application logging started'); // System category
    }

    // UI layer logging
    logUI(level, message, ...args) {
        if (this.logger) {
            this.logger[this._getLevelMethod(level)](0, `[UI] ${message}`, ...args);
        }
    }

    // Business logic logging
    logBusiness(level, message, ...args) {
        if (this.logger) {
            this.logger[this._getLevelMethod(level)](1, `[Business] ${message}`, ...args);
        }
    }

    // Network operations logging
    logNetwork(level, message, ...args) {
        if (this.logger) {
            this.logger[this._getLevelMethod(level)](2, `[Network] ${message}`, ...args);
        }
    }

    // Storage operations logging
    logStorage(level, message, ...args) {
        if (this.logger) {
            this.logger[this._getLevelMethod(level)](3, `[Storage] ${message}`, ...args);
        }
    }

    // System level logging
    logSystem(level, message, ...args) {
        if (this.logger) {
            this.logger[this._getLevelMethod(level)](4, `[System] ${message}`, ...args);
        }
    }

    _getLevelMethod(level) {
        switch(level) {
            case LogLevel.VERBOSE: return 'verbose';
            case LogLevel.DEBUG: return 'debug';
            case LogLevel.INFO: return 'info';
            case LogLevel.WARNING: return 'warning';
            case LogLevel.ERROR: return 'error';
            case LogLevel.FATAL: return 'fatal';
            default: return 'info';
        }
    }

    // Flush logs (important for HarmonyOS lifecycle)
    flush() {
        if (this.logger) {
            this.logger.forceFlush();
        }
    }

    // Get log snapshot for debugging
    getSnapshot(useGMT = false) {
        if (this.logger) {
            return this.logger.takeSnapshotString(useGMT);
        }
        return null;
    }

    // Cleanup (important for HarmonyOS resource management)
    cleanup() {
        if (this.logger) {
            this.logger.forceFlush();
            console.log('HarmonyOS Logger cleaned up');
        }
    }
}

// Example usage in HarmonyOS application
async function harmonyOSExample() {
    try {
        // Initialize logger for HarmonyOS app
        const harmonyLogger = new HarmonyOSLogger('MyHarmonyApp');
        
        // Simulate HarmonyOS application lifecycle
        harmonyLogger.logSystem(LogLevel.INFO, 'Application onCreate');
        
        // UI operations
        harmonyLogger.logUI(LogLevel.INFO, 'Main page loaded');
        harmonyLogger.logUI(LogLevel.DEBUG, 'Button clicked: {0}', 'login_button');
        
        // Business logic
        harmonyLogger.logBusiness(LogLevel.INFO, 'User authentication started');
        harmonyLogger.logBusiness(LogLevel.WARNING, 'Authentication took longer than expected: {0}ms', 3000);
        
        // Network operations
        harmonyLogger.logNetwork(LogLevel.INFO, 'API request: {0}', '/api/user/profile');
        harmonyLogger.logNetwork(LogLevel.ERROR, 'Network error: {0}', 'Connection timeout');
        
        // Storage operations
        harmonyLogger.logStorage(LogLevel.INFO, 'Saving user preferences');
        harmonyLogger.logStorage(LogLevel.DEBUG, 'Cache size: {0} KB', 1024);
        
        // System events
        harmonyLogger.logSystem(LogLevel.INFO, 'Application onPause');
        harmonyLogger.logSystem(LogLevel.INFO, 'Application onResume');
        
        // Force flush before cleanup (important for HarmonyOS)
        harmonyLogger.flush();
        
        // Get snapshot for debugging
        const snapshot = harmonyLogger.getSnapshot();
        if (snapshot) {
            console.log(`Log snapshot captured: ${snapshot.length} characters`);
        }
        
        // Cleanup resources
        harmonyLogger.cleanup();
        
        console.log('HarmonyOS example completed successfully');
        
    } catch (error) {
        console.error('HarmonyOS example failed:', error);
    }
}

// Export for use in HarmonyOS applications
module.exports = {
    HarmonyOSLogger,
    harmonyOSExample
};

// Run example if this file is executed directly
if (require.main === module) {
    harmonyOSExample().catch(console.error);
}