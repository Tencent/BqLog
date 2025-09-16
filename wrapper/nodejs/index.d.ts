/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 */

export enum LogLevel {
    VERBOSE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    FATAL = 5
}

export interface ConsoleCallbackData {
    logId: number;
    categoryIdx: number;
    logLevel: number;
    content: string;
}

export type ConsoleCallback = (data: ConsoleCallbackData) => void;

export declare class BqLog {
    constructor(logId?: number);
    
    // Instance methods
    resetConfig(logName: string, config: string): void;
    setAppendersEnable(appenderName: string, enable: boolean): void;
    forceFlush(): void;
    log(categoryIdx: number, level: number, message: string): void;
    setConsoleCallback(callback: ConsoleCallback): void;
    takeSnapshotString(useGmtTime?: boolean): string | null;
    isValid(): boolean;
    getLogId(): number;

    // Convenience logging methods
    verbose(categoryIdx: number, message: string, ...args: any[]): void;
    debug(categoryIdx: number, message: string, ...args: any[]): void;
    info(categoryIdx: number, message: string, ...args: any[]): void;
    warning(categoryIdx: number, message: string, ...args: any[]): void;
    error(categoryIdx: number, message: string, ...args: any[]): void;
    fatal(categoryIdx: number, message: string, ...args: any[]): void;

    // Static methods
    static createLog(logName: string, config: string, categories?: string[]): BqLog | null;
    static getLogByName(logName: string): BqLog | null;
    static getVersion(): string;
    static enableAutoCrashHandler(): void;
    static uninit(): void;

    readonly logLevel: typeof LogLevel;
}

export declare class BqLogNative {
    // Native binding interface (without convenience methods)
    constructor(logId?: number);
    
    resetConfig(logName: string, config: string): void;
    setAppendersEnable(appenderName: string, enable: boolean): void;
    forceFlush(): void;
    log(categoryIdx: number, level: number, message: string): void;
    setConsoleCallback(callback: ConsoleCallback): void;
    takeSnapshotString(useGmtTime?: boolean): string | null;
    isValid(): boolean;
    getLogId(): number;

    static createLog(logName: string, config: string, categories?: string[]): BqLogNative | null;
    static getLogByName(logName: string): BqLogNative | null;
    static getVersion(): string;
    static enableAutoCrashHandler(): void;
    static uninit(): void;
}

export { BqLog as default };