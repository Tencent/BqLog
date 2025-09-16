/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 */

const { BqLog, LogLevel } = require('../index');

// Configuration with console appender
const config = JSON.stringify({
    "appenders_config": [
        {
            "type": "console",
            "name": "console_appender",
            "levels": ["all"],
            "enable": true
        }
    ]
});

async function consoleCallbackExample() {
    console.log('Console Callback Example');
    
    const logger = BqLog.createLog('CallbackTest', config, ['Main']);
    
    if (!logger || !logger.isValid()) {
        console.error('Failed to create logger');
        return;
    }
    
    // Set up console callback to intercept log output
    logger.setConsoleCallback((data) => {
        console.log('=== Console Callback ===');
        console.log('Log ID:', data.logId);
        console.log('Category Index:', data.categoryIdx);
        console.log('Level:', data.logLevel);
        console.log('Content:', data.content);
        console.log('========================');
    });
    
    // Generate some log messages
    logger.info(0, 'This message will trigger the callback');
    logger.warning(0, 'Warning message with callback');
    logger.error(0, 'Error message: {0}', 'Something went wrong');
    
    // Wait a bit for async callbacks
    await new Promise(resolve => setTimeout(resolve, 100));
    
    console.log('Console callback example completed');
}

// Run the example
consoleCallbackExample().catch(console.error);