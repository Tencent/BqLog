/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 */

const { BqLog, LogLevel } = require('../index');

// Basic configuration for console and file output
const config = JSON.stringify({
    "appenders_config": [
        {
            "type": "console",
            "name": "console_appender",
            "levels": ["all"],
            "enable": true
        },
        {
            "type": "file",
            "name": "file_appender", 
            "file_name": "./logs/app.log",
            "levels": ["all"],
            "enable": true
        }
    ]
});

async function basicExample() {
    console.log('BqLog Version:', BqLog.getVersion());
    
    // Enable auto crash handler for better crash recovery
    BqLog.enableAutoCrashHandler();
    
    // Create a log with categories
    const categories = ['General', 'Network', 'Database'];
    const logger = BqLog.createLog('MyApp', config, categories);
    
    if (!logger || !logger.isValid()) {
        console.error('Failed to create logger');
        return;
    }
    
    console.log('Logger created with ID:', logger.getLogId());
    
    // Log messages with different levels
    logger.info(0, 'Application started');
    logger.debug(1, 'Network connection established');
    logger.warning(2, 'Database connection slow: {0}ms', 150);
    logger.error(0, 'An error occurred: {0}', 'File not found');
    
    // Using verbose and fatal levels
    logger.verbose(0, 'Verbose debug information');
    logger.fatal(0, 'Critical system failure');
    
    // Force flush to ensure all logs are written
    logger.forceFlush();
    
    // Get a snapshot of current logs
    const snapshot = logger.takeSnapshotString(false);
    if (snapshot) {
        console.log('Log snapshot length:', snapshot.length);
    }
    
    console.log('Basic example completed');
}

// Run the example
basicExample().catch(console.error);