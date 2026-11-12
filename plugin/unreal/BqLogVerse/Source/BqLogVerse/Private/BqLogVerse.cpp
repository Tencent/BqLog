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
#include "BqLogVerse.h"
#include "BqLogVerse.bq.gen.h"

// Each generated .ipp registers native types or functions (V_IMPLEMENT_CLASS, the module
// bindings) and must be compiled exactly once, so they are included here and nowhere else.
#include "BqLog.bq.log.gen.ipp"
#include "BqLog.bq.category_log.gen.ipp"
#include "BqLogVerse.bq.gen.ipp"

#include "Containers/Map.h"
#include "Templates/UniquePtr.h"
#include "VerseVM/VVMContentScope.h"
#include "bq_log/bq_log.h"

#include <string_view>

DEFINE_LOG_CATEGORY_STATIC(LogBqLogVerse, Log, All);

namespace
{
	static_assert(static_cast<int32>(bq::log_level::verbose) == 0 && static_cast<int32>(bq::log_level::fatal) == 5,
		"the category logs BqLog_CategoryLogGenerator writes for Verse pass levels as 0 (verbose) to 5 (fatal)");

	// One bq::log per log id, shared by every Verse object of that log. BqLog never destroys
	// a log object, so the handles stay valid for the life of the process.
	TMap<uint64, TUniquePtr<bq::log>>& GetLogHandles()
	{
		static TMap<uint64, TUniquePtr<bq::log>> Handles;
		return Handles;
	}

	// Verse strings are UTF-8, like every string BqLog takes. The text is zero-terminated.
	const char* Utf8(const verse::string& Str)
	{
		return reinterpret_cast<const char*>(*Str);
	}

	verse::string ToVerse(const char* Str, uint32 Len)
	{
		return verse::string(FUtf8StringView(reinterpret_cast<const UTF8CHAR*>(Str), static_cast<int32>(Len)));
	}

	verse::string ToVerse(const bq::string& Str)
	{
		return ToVerse(Str.c_str(), static_cast<uint32>(Str.size()));
	}

	verse::string ToVerse(const bq::_api_string_def& Def)
	{
		const char* Str = Def.str; // packed struct: copy the fields before use
		const uint32 Len = Def.len;
		return Str ? ToVerse(Str, Len) : verse::string();
	}

	bool NameEquals(const bq::_api_string_def& Def, const char* Name, uint32 NameLen)
	{
		const char* Str = Def.str;
		const uint32 Len = Def.len;
		return Str != nullptr && Len == NameLen && (NameLen == 0 || FMemory::Memcmp(Str, Name, NameLen) == 0);
	}

	bool NameEquals(const bq::_api_string_def& Def, const verse::string& Name)
	{
		return NameEquals(Def, Utf8(Name), static_cast<uint32>(Name.Len()));
	}

	// The handle of log Id, made on first use; null if there is no such log.
	bq::log* GetHandle(uint64 Id)
	{
		if (Id == 0)
		{
			return nullptr;
		}
		if (const TUniquePtr<bq::log>* Found = GetLogHandles().Find(Id))
		{
			return (*Found)->is_valid() ? Found->Get() : nullptr;
		}
		bq::_api_string_def Name;
		if (!bq::api::__api_get_log_name_by_id(Id, &Name))
		{
			return nullptr;
		}
		const char* InternalName = Name.str;
		TUniquePtr<bq::log>& Handle = GetLogHandles().Add(Id, MakeUnique<bq::log>(bq::log::get_log_by_name(bq::string(InternalName))));
		return Handle->is_valid() ? Handle.Get() : nullptr;
	}

	uint64 FindLogId(const verse::string& Name)
	{
		const uint32 Count = bq::api::__api_get_logs_count();
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			const uint64 Id = bq::api::__api_get_log_id_by_index(Index);
			bq::_api_string_def LogName;
			if (Id != 0 && bq::api::__api_get_log_name_by_id(Id, &LogName) && NameEquals(LogName, Name))
			{
				return Id;
			}
		}
		return 0;
	}

	// True if log LogId has exactly these categories, in this order.
	bool HasCategories(uint64 LogId, const TArray<const char*>& Names)
	{
		if (bq::api::__api_get_log_categories_count(LogId) != static_cast<uint32>(Names.Num()))
		{
			return false;
		}
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			bq::_api_string_def CategoryName;
			if (!bq::api::__api_get_log_category_name_by_index(LogId, static_cast<uint32>(Index), &CategoryName)
				|| !NameEquals(CategoryName, Names[Index], static_cast<uint32>(FCStringAnsi::Strlen(Names[Index]))))
			{
				return false;
			}
		}
		return true;
	}

	// The names as BqLog takes them; the text stays owned by Names.
	TArray<const char*> ToUtf8(const TArray<verse::string>& Names)
	{
		TArray<const char*> Result;
		Result.Reserve(Names.Num());
		for (const verse::string& Name : Names)
		{
			Result.Add(Utf8(Name));
		}
		return Result;
	}

	// create_log as in C++: creates the log named Name, or applies Config to the log of that
	// name. Null on failure. A log keeps its categories for life, and BqLog asserts when it is
	// given an existing log with other ones; from Verse that is a failure with a warning instead.
	bq::log* CreateWithCategories(const verse::string& Name, const verse::string& Config, const TArray<const char*>& CategoryNames)
	{
		if (const uint64 ExistingId = Name.IsEmpty() ? 0 : FindLogId(Name))
		{
			if (!HasCategories(ExistingId, CategoryNames))
			{
				UE_LOGF(LogBqLogVerse, Warning,
					"create_log: log \"%.*s\" already exists with other categories, and a log's categories cannot change",
					Name.Len(), *Name);
				return nullptr;
			}
		}
		// On failure (a configuration that does not parse, say) BqLog reports why itself.
		return GetHandle(bq::api::__api_create_log(Utf8(Name), Utf8(Config), static_cast<uint32>(CategoryNames.Num()), CategoryNames.GetData()));
	}

	// Verse has finished any interpolation by now, so the message goes out as the single
	// argument of a constant format: braces inside it are never read as placeholders, and the
	// UTF-8 bytes are copied once, straight into the log buffer. do_log checks the level first,
	// as every level function of bq::log does.
	void WriteMessage(const bq::log* Log, uint32 CategoryIndex, bq::log_level Level, const verse::string& Message)
	{
		if (Log)
		{
			Log->do_log(CategoryIndex, Level, "{}", std::string_view(Utf8(Message), static_cast<size_t>(Message.Len())));
		}
	}

	TOptional<EVerseTrue> Succeeds(bool bSucceeds)
	{
		return bSucceeds ? TOptional<EVerseTrue>{EVerseTrue::Value} : TOptional<EVerseTrue>{};
	}
}

namespace verse
{
	TNonNullPtr<bq_log> bq_log::Make(::bq::log& InLog)
	{
		bq_log* Result = NewObject<bq_log>(verse::GetInstantiationOuter(), NAME_None, verse::GetInstantiationFlags());
		Result->Log = &InLog;
		return TNonNullPtr<bq_log>(Result);
	}

	TOptional<EVerseTrue> bq_log::is_valid()
	{
		return Succeeds(Log != nullptr && Log->is_valid());
	}

	verse::string bq_log::get_name()
	{
		return Log ? ToVerse(Log->get_name()) : verse::string();
	}

	int64 bq_log::get_id()
	{
		return Log ? static_cast<int64>(Log->get_id()) : 0;
	}

	TOptional<EVerseTrue> bq_log::reset_config(verse::string const& Config)
	{
		// BqLog itself reports a configuration that does not parse.
		return Succeeds(Log != nullptr && Log->reset_config(::bq::string(Utf8(Config))));
	}

	void bq_log::set_appender_enable(verse::string const& AppenderName, bool Enable)
	{
		if (Log)
		{
			Log->set_appender_enable(::bq::string(Utf8(AppenderName)), Enable);
		}
	}

	void bq_log::force_flush()
	{
		if (Log)
		{
			Log->force_flush();
		}
	}

	verse::string bq_log::take_snapshot(verse::string const& TimeZone)
	{
		return Log ? ToVerse(Log->take_snapshot(::bq::string(Utf8(TimeZone)))) : verse::string();
	}

	void bq_log::verbose(verse::string const& Message)
	{
		WriteMessage(Log, 0, ::bq::log_level::verbose, Message);
	}

	void bq_log::debug(verse::string const& Message)
	{
		WriteMessage(Log, 0, ::bq::log_level::debug, Message);
	}

	void bq_log::info(verse::string const& Message)
	{
		WriteMessage(Log, 0, ::bq::log_level::info, Message);
	}

	void bq_log::warning(verse::string const& Message)
	{
		WriteMessage(Log, 0, ::bq::log_level::warning, Message);
	}

	void bq_log::error(verse::string const& Message)
	{
		WriteMessage(Log, 0, ::bq::log_level::error, Message);
	}

	void bq_log::fatal(verse::string const& Message)
	{
		WriteMessage(Log, 0, ::bq::log_level::fatal, Message);
	}

	bool bq_category_log::Bind(::bq::log* InLog, int32 InCategoriesCount)
	{
		if (!InLog)
		{
			return false;
		}
		Log = InLog;
		CategoriesCount = InCategoriesCount;
		return true;
	}

	int64 bq_category_log::get_categories_count()
	{
		return CategoriesCount;
	}

	TArray<verse::string> bq_category_log::get_categories_name_array()
	{
		TArray<verse::string> Names;
		if (Log)
		{
			Names.Reserve(CategoriesCount);
			for (int32 Index = 0; Index < CategoriesCount; ++Index)
			{
				::bq::_api_string_def CategoryName;
				Names.Add(::bq::api::__api_get_log_category_name_by_index(Log->get_id(), static_cast<uint32>(Index), &CategoryName) ? ToVerse(CategoryName) : verse::string());
			}
		}
		return Names;
	}

	void bq_category_log::do_log(int64 CategoryIndex, int64 Level, verse::string const& Message)
	{
		// The generated classes pass the index of one of their categories, which the bound log
		// has (create_and_bind and find_and_bind make sure of that), and a level from 0 to 5.
		if (CategoryIndex >= 0 && CategoryIndex < CategoriesCount
			&& Level >= static_cast<int64>(::bq::log_level::verbose) && Level <= static_cast<int64>(::bq::log_level::fatal))
		{
			WriteMessage(Log, static_cast<uint32>(CategoryIndex), static_cast<::bq::log_level>(Level), Message);
		}
	}

	TOptional<EVerseTrue> bq_category_log::create_and_bind(verse::string const& Name, verse::string const& Config, TArray<verse::string> const& CategoryNames)
	{
		const TArray<const char*> Names = ToUtf8(CategoryNames);
		return Succeeds(Bind(CreateWithCategories(Name, Config, Names), Names.Num()));
	}

	TOptional<EVerseTrue> bq_category_log::find_and_bind(verse::string const& Name, TArray<verse::string> const& CategoryNames)
	{
		// As get_log_by_name of a generated class in C++: only a log with exactly these categories.
		const uint64 Id = FindLogId(Name);
		return Succeeds(Id != 0 && HasCategories(Id, ToUtf8(CategoryNames)) && Bind(GetHandle(Id), CategoryNames.Num()));
	}
}

// The functions of the module bq in /Tencent.com/BqLog.
namespace verse::bq
{
	TOptional<TNonNullPtr<verse::bq_log>> create_log(verse::string const& Name, verse::string const& Config)
	{
		// As create_log in C++: only the default category.
		const TArray<const char*> DefaultCategoryOnly{""};
		if (::bq::log* Log = CreateWithCategories(Name, Config, DefaultCategoryOnly))
		{
			return TOptional<TNonNullPtr<verse::bq_log>>(verse::bq_log::Make(*Log));
		}
		return TOptional<TNonNullPtr<verse::bq_log>>();
	}

	TOptional<TNonNullPtr<verse::bq_log>> get_log_by_name(verse::string const& Name)
	{
		if (::bq::log* Log = GetHandle(FindLogId(Name)))
		{
			return TOptional<TNonNullPtr<verse::bq_log>>(verse::bq_log::Make(*Log));
		}
		return TOptional<TNonNullPtr<verse::bq_log>>();
	}

	void force_flush_all_logs()
	{
		::bq::log::force_flush_all_logs();
	}

	verse::string get_version()
	{
		return ToVerse(::bq::log::get_version());
	}
}
