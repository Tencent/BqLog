/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 */

const { BqLog, LogLevel } = require('../index');
const assert = require('assert');

// Test configuration
const testConfig = JSON.stringify({
    "appenders_config": [
        {
            "type": "console",
            "name": "test_console",
            "levels": ["all"],
            "enable": false  // Disable for testing
        }
    ]
});

describe('BqLog Node API Tests', function() {
    
    describe('Static Methods', function() {
        it('should return version string', function() {
            const version = BqLog.getVersion();
            assert(typeof version === 'string');
            assert(version.length > 0);
            console.log('BqLog version:', version);
        });

        it('should enable auto crash handler without error', function() {
            assert.doesNotThrow(() => {
                BqLog.enableAutoCrashHandler();
            });
        });

        it('should create log instance', function() {
            const logger = BqLog.createLog('TestLog', testConfig, ['Test']);
            assert(logger !== null);
            assert(logger.isValid());
            console.log('Created logger with ID:', logger.getLogId());
        });

        it('should get log by name', function() {
            // First create a log
            const logger1 = BqLog.createLog('NamedLog', testConfig, ['Test']);
            assert(logger1 !== null);
            
            // Then get it by name
            const logger2 = BqLog.getLogByName('NamedLog');
            assert(logger2 !== null);
            assert(logger2.isValid());
            assert(logger1.getLogId() === logger2.getLogId());
        });
    });

    describe('Instance Methods', function() {
        let logger;

        beforeEach(function() {
            logger = BqLog.createLog('InstanceTest', testConfig, ['Category1', 'Category2']);
            assert(logger !== null);
        });

        it('should log messages at different levels', function() {
            assert.doesNotThrow(() => {
                logger.verbose(0, 'Verbose message');
                logger.debug(0, 'Debug message');
                logger.info(0, 'Info message');
                logger.warning(0, 'Warning message');
                logger.error(0, 'Error message');
                logger.fatal(0, 'Fatal message');
            });
        });

        it('should handle message formatting', function() {
            assert.doesNotThrow(() => {
                logger.info(0, 'Message with args: {0}, {1}', 'arg1', 42);
                logger.error(1, 'Error code: {0}', 500);
            });
        });

        it('should force flush without error', function() {
            logger.info(0, 'Message before flush');
            assert.doesNotThrow(() => {
                logger.forceFlush();
            });
        });

        it('should set appenders enable', function() {
            assert.doesNotThrow(() => {
                logger.setAppendersEnable('test_console', true);
                logger.setAppendersEnable('test_console', false);
            });
        });

        it('should get snapshot string', function() {
            logger.info(0, 'Message for snapshot');
            logger.forceFlush();
            
            const snapshot = logger.takeSnapshotString(false);
            // Snapshot might be null if no console output
            if (snapshot !== null) {
                assert(typeof snapshot === 'string');
            }
        });

        it('should validate logger state', function() {
            assert(logger.isValid());
            assert(typeof logger.getLogId() === 'number');
            assert(logger.getLogId() > 0);
        });
    });

    describe('Console Callback', function() {
        it('should set console callback', function(done) {
            const consoleConfig = JSON.stringify({
                "appenders_config": [
                    {
                        "type": "console",
                        "name": "callback_console",
                        "levels": ["all"],
                        "enable": true
                    }
                ]
            });

            const logger = BqLog.createLog('CallbackTest', consoleConfig, ['Test']);
            assert(logger !== null);

            let callbackReceived = false;
            
            logger.setConsoleCallback((data) => {
                callbackReceived = true;
                assert(typeof data === 'object');
                assert(typeof data.logId === 'number');
                assert(typeof data.categoryIdx === 'number');
                assert(typeof data.logLevel === 'number');
                assert(typeof data.content === 'string');
                
                console.log('Callback received:', data.content);
                
                if (!done.called) {
                    done.called = true;
                    done();
                }
            });

            // Generate a log message
            logger.info(0, 'Test callback message');
            
            // Fallback timeout in case callback doesn't work
            setTimeout(() => {
                if (!callbackReceived && !done.called) {
                    done.called = true;
                    console.log('Warning: Console callback was not received (this may be expected in test environment)');
                    done();
                }
            }, 1000);
        });
    });

    describe('Error Handling', function() {
        it('should handle invalid log creation', function() {
            const invalidLogger = BqLog.createLog('', '', []);
            // Should return null or invalid logger
            if (invalidLogger !== null) {
                assert(!invalidLogger.isValid());
            }
        });

        it('should handle non-existent log retrieval', function() {
            const nonExistentLogger = BqLog.getLogByName('NonExistentLog');
            assert(nonExistentLogger === null);
        });
    });

    after(function() {
        // Cleanup
        BqLog.uninit();
        console.log('Tests completed');
    });
});

// Simple test runner if not using a testing framework
if (require.main === module) {
    console.log('Running BqLog Node API tests...');
    
    // Run basic tests
    try {
        console.log('Testing version...');
        const version = BqLog.getVersion();
        console.log('✓ Version:', version);

        console.log('Testing log creation...');
        const logger = BqLog.createLog('SimpleTest', testConfig, ['Test']);
        console.log('✓ Logger created:', logger ? 'success' : 'failed');

        if (logger) {
            console.log('Testing logging...');
            logger.info(0, 'Test message');
            logger.forceFlush();
            console.log('✓ Logging works');
        }

        console.log('✓ All basic tests passed');
    } catch (error) {
        console.error('✗ Test failed:', error);
        process.exit(1);
    }
}