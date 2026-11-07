# Copyright (C) 2025 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.

using { /Verse.org/Native }

<#
The Verse API of BqLog, with the names of the C++ API: bq.log is bq::log, bq.create_log is
bq::log::create_log, and so on.

    using { /Tencent.com/BqLog }

    game_logic := class:
        Start()<transacts>:void =
            Config := "appenders_config.file.type=compressed_file\nappenders_config.file.file_name=Game\n"
            if (GameLog := bq.create_log["Game", Config]):
                GameLog.info("game started")

        OnJoin(Name:string)<transacts>:void =
            # Any code can look a log up by name, whoever created it: Verse, C++, Blueprint or a BqLog asset.
            if (GameLog := bq.get_log_by_name["Game"]):
                GameLog.debug("player {Name} joined")

A log with categories is a class BqLog_CategoryLogGenerator writes for Verse (<ClassName>.verse),
used as in the other languages: MyLog.info(MyLog.cat.Net.Socket, "..."). Do not call a variable
Log: the editor reports it as ambiguous with the function Log of /Verse.org/Verse. See
docs/ENGINE_INTEGRATION.md.
#>

bq<public> := module:

    @doc("A BqLog log object, like bq::log in C++. It comes from create_log or get_log_by_name. Each level function checks the level itself. Messages are written as they are: braces in them are not format placeholders.")
    @cpp_name("bq_log")
    log<native><public> := class<computes><internal>:

        @doc("Succeeds if the log object is valid.")
        is_valid<native><public>()<computes><decides>:void

        @doc("Name of the log.")
        get_name<native><public>()<computes>:string

        @doc("Id of the log.")
        get_id<native><public>()<computes>:int

        @doc("Applies a new configuration to the log. Some settings, such as buffer sizes, keep the value the log was created with. Fails if Config cannot be parsed.")
        reset_config<native><public>(Config:string)<transacts><decides>:void

        @doc("Turns the appender named AppenderName (the name in appenders_config.<name>) on or off.")
        set_appender_enable<native><public>(AppenderName:string, Enable:logic)<transacts>:void

        @doc("Blocks until every entry already written to this log has been processed by its appenders.")
        force_flush<native><public>()<computes>:void

        @doc("The entries in the snapshot buffer as text, with times shown in TimeZone (\"localtime\", \"UTC\", \"UTC+8\" ...). Empty unless the log configures snapshot.buffer_size.")
        take_snapshot<native><public>(TimeZone:string)<reads>:string

        verbose<native><public>(Message:string)<computes>:void
        debug<native><public>(Message:string)<computes>:void
        info<native><public>(Message:string)<computes>:void
        warning<native><public>(Message:string)<computes>:void
        error<native><public>(Message:string)<computes>:void
        fatal<native><public>(Message:string)<computes>:void

    @doc("Base class of the category logs BqLog_CategoryLogGenerator writes for Verse, like bq::category_log in C++.")
    @cpp_name("bq_category_log")
    category_log<native><public> := class<abstract><computes>(log):

        @doc("Number of categories, the default category included.")
        get_categories_count<native><public>()<computes>:int

        @doc("Names of the categories in index order, the default category \"\" first.")
        get_categories_name_array<native><public>()<computes>:[]string

        # For the generated classes. Level is a bq::log_level value: 0 verbose ... 5 fatal.
        do_log<native><protected>(CategoryIndex:int, Level:int, Message:string)<computes>:void
        create_and_bind<native><protected>(Name:string, Config:string, CategoryNames:[]string)<transacts><decides>:void
        find_and_bind<native><protected>(Name:string, CategoryNames:[]string)<reads><decides>:void

    @doc("Creates the log named Name from Config; if a log of that name exists, Config is applied to it instead. An empty Name gets a generated one. Fails if Config cannot be parsed, or if the existing log has categories.")
    create_log<native><public>(Name:string, Config:string)<transacts><decides>:log

    @doc("The log named Name, whoever created it. Fails if there is none.")
    get_log_by_name<native><public>(Name:string)<reads><decides>:log

    @doc("Blocks until every entry already written to any log has been processed by its appenders.")
    force_flush_all_logs<native><public>()<computes>:void

    @doc("The BqLog version, such as 2.8.0.")
    get_version<native><public>()<computes>:string
