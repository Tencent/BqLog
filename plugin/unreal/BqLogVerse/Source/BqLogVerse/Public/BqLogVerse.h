/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
/*!
 * Native side of the Verse package /Tencent.com/BqLog (Verse/BqLog.native.verse).
 *
 * The declarations below are dictated by the code VNI generates from the .verse file:
 * method signatures are checked with static_assert (non-const, exact parameter types).
 * bq.log and bq.category_log are verse::bq_log and verse::bq_category_log, named by their
 * @cpp_name; the functions of the module bq (create_log, get_log_by_name ...) are declared
 * by BqLogVerse.bq.gen.h, in namespace verse::bq.
 *
 * Inside namespace verse, "bq" names that module namespace, so BqLog's own namespace is
 * always written ::bq here.
 */
#pragma once

#include "CoreMinimal.h"
#include "VerseString.h"

#include "BqLog.bq.log.gen.h"
#include "BqLog.bq.category_log.gen.h"

namespace bq
{
	class log;
}

namespace verse
{
	class bq_log : public UObject
	{
		BQ_LOG_BODY()

	public:
		// Verse API
		TOptional<EVerseTrue> is_valid();
		verse::string get_name();
		int64 get_id();
		TOptional<EVerseTrue> reset_config(verse::string const& Config);
		void set_appender_enable(verse::string const& AppenderName, bool Enable);
		void force_flush();
		verse::string take_snapshot(verse::string const& TimeZone);
		void verbose(verse::string const& Message);
		void debug(verse::string const& Message);
		void info(verse::string const& Message);
		void warning(verse::string const& Message);
		void error(verse::string const& Message);
		void fatal(verse::string const& Message);

		// For the functions of the module bq: a new bq_log for Log, one of the handles the
		// module keeps.
		static TNonNullPtr<bq_log> Make(::bq::log& Log);

	protected:
		// Shared per log id and owned by the module; a log object lives as long as the process.
		// Null only in a category log that create_and_bind or find_and_bind has not bound.
		::bq::log* Log = nullptr;
	};

	class bq_category_log : public bq_log
	{
		BQ_CATEGORY_LOG_BODY()

	public:
		// Verse API
		int64 get_categories_count();
		TArray<verse::string> get_categories_name_array();
		void do_log(int64 CategoryIndex, int64 Level, verse::string const& Message);
		TOptional<EVerseTrue> create_and_bind(verse::string const& Name, verse::string const& Config, TArray<verse::string> const& CategoryNames);
		TOptional<EVerseTrue> find_and_bind(verse::string const& Name, TArray<verse::string> const& CategoryNames);

	private:
		bool Bind(::bq::log* InLog, int32 InCategoriesCount);

		int32 CategoriesCount = 0;
	};
}
