// gettext for 3ds, Copyright (C) 2024 R-YaTian
// Ported from ftpd, Copyright (C) 2024 Michael Theall
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://gnu.org>.

#include "3dsgettext.h"

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
	using StringMap = std::map<std::string, std::string>;
	using Table     = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

	// return 0; 後のグローバルデストラクタによるヒープ破壊を防ぐためポインタで保持
	StringMap *s_translations_ptr = nullptr;

	StringMap &getTranslations()
	{
		if (!s_translations_ptr)
			s_translations_ptr = new StringMap();
		return *s_translations_ptr;
	}

	/// \brief Read u32 value
	/// \param file_ File to read
	/// \param[out] data_ Output data
	/// \param le_ Whether little-endian
	bool read(FILE* file_, std::uint32_t &data_, bool const le_)
	{
		if (!fread(&data_, sizeof(uint8_t), sizeof(data_), file_))
			return false;

		if (!le_)
			data_ = __builtin_bswap32(data_);

		return true;
	}

	/// \brief Load string table [offset, length]
	/// \param file_ File to read
	/// \param offset_ Offset to read from
	/// \param count_ Number of entries
	/// \param le_ Whether little-endian
	Table loadTable (FILE* file_, off_t const offset_, std::uint32_t const count_, bool const le_)
	{
		if (fseek(file_, offset_, SEEK_SET) != 0)
			return {};

		Table table;
		for (std::uint32_t i = 0; i < count_; ++i)
		{
			std::uint32_t length;
			if (!read(file_, length, le_))
				return {};

			std::uint32_t offset;
			if (!read(file_, offset, le_))
				return {};

			table.emplace_back(std::make_pair(offset, length));
		}

		return table;
	}

	/// \brief Load string
	/// \param file_ File to read
	/// \param offset_ String offset
	/// \param length_ String length
	std::string loadString(FILE* file_, std::uint32_t const offset_, std::uint32_t const length_)
	{
		if (length_ == 0)
			return {};

		if (fseek(file_, offset_, SEEK_SET) != 0)
			return {};

		std::string str(length_, 0);

		if (!fread(&str[0], sizeof(uint8_t), length_, file_))
			return {};

		return str;
	}

	/// \brief Load translations
	/// \param language_ Language to load
	void loadTranslations(char const *const language_)
	{
		StringMap &translations = getTranslations();

		char path[64];
		std::snprintf(path, sizeof(path), "romfs:/locale/%s/emus3ds.mo", language_);

		FILE *file = fopen(path, "rb");
		if (file == NULL) {
			// romfsに.moファイルが無い場合でも主要UIを英語化する
			translations["选项"] = "Options";
			translations["选择 ROM"] = "Select ROM";
			translations["选择ROM"] = "Select ROM";
			translations["  语言"] = "  Language";
			translations["选择应用程序显示的语言"] = "Select the language for the UI";
			translations["  主题"] = "  Theme";
			translations["选择应用于界面的主题"] = "Select the theme for the UI";
			translations["  字体"] = "  Font";
			translations["用于用户界面的字体(仅适用于ASCII字符)"] = "Font for UI (ASCII characters only)";
			translations["  退出"] = "  Exit";
			translations["退出"] = "Exit";
			translations["立即退出?"] = "Exit now?";
			translations["原始"] = "Original";
			translations["暗黑模式"] = "Dark Mode";
			translations["否"] = "No";
			translations["是"] = "Yes";
			translations["确定"] = "OK";
			translations["A:选择  B:取消  X+上/下:翻页"] = "A:Select B:Back X+Up/Down:Page";
			translations["模拟器"] = "Emulator";
			translations["设置"] = "Settings";
			translations["控制"] = "Controls";
			translations["金手指"] = "Cheats";
			translations["加载 ROM"] = "Load ROM";
			translations["加载中... 请稍后."] = "Loading... Please wait.";
			translations["无法加载 ROM."] = "Unable to load ROM.";
			translations["保存设置到SD卡..."] = "Saving settings to SD card...";
			translations["即时存档"] = "Save State";
			translations["自动保存..."] = "Auto-saving...";
			translations["自动保存失败"] = "Auto-save failed";
			translations["自动保存失败.\n强制加载?"] = "Auto-save failed.\nForce load?";
			translations["截屏"] = "Screenshot";
			translations["开始截屏...\n请稍后."] = "Taking screenshot...\nPlease wait.";
			translations["截屏时发生错误!"] = "Error taking screenshot!";
			translations["重置控制台"] = "Reset Console";
			translations["确定吗?"] = "Are you sure?";
			translations["无此游戏可用的金手指."] = "No cheats available for this game.";
			translations["此游戏没有可用的金手指."] = "No cheats available for this game.";
			return;
		}

		std::uint32_t magic;
		if (!read(file, magic, true))
		{
			fclose(file);
			return;
		}

		bool le;
		if (magic == 0x950412DE)
			le = true;
		else if (magic == 0xDE120495)
			le = false;
		else
		{
			fclose(file);
			return;
		}

		std::uint32_t version;
		if (!read(file, version, le) || version != 0)
		{
			fclose(file);
			return;
		}

		std::uint32_t numStrings;
		if (!read(file, numStrings, le))
		{
			fclose(file);
			return;
		}

		std::uint32_t originalOffset;
		if (!read(file, originalOffset, le))
		{
			fclose(file);
			return;
		}

		std::uint32_t translationOffset;
		if (!read(file, translationOffset, le))
		{
			fclose(file);
			return;
		}

		std::uint32_t hashSize;
		if (!read(file, hashSize, le))
		{
			fclose(file);
			return;
		}

		std::uint32_t hashOffset;
		if (!read(file, hashOffset, le))
		{
			fclose(file);
			return;
		}

		auto originalTable = loadTable(file, originalOffset, numStrings, le);
		if (originalTable.size() != numStrings)
		{
			fclose(file);
			return;
		}

		auto translationTable = loadTable(file, translationOffset, numStrings, le);
		if (translationTable.size() != numStrings)
		{
			fclose(file);
			return;
		}

		StringMap strings;
		for (std::uint32_t i = 0; i < numStrings; ++i)
		{
			auto originalString = loadString(file, originalTable[i].first, originalTable[i].second);
			auto translationString =
				loadString(file, translationTable[i].first, translationTable[i].second);

			strings.emplace(std::make_pair(originalString, translationString));
		}

		fclose(file);
		std::swap(strings, translations);
	}
}

void setLanguage(char const *const language_)
{
	if (std::strcmp(language_, "zh_Hans") == 0)
	{
		if (s_translations_ptr && !s_translations_ptr->empty())
			s_translations_ptr->clear();
	}
	else
	{
		loadTranslations(language_);
	}
}

char const *getText(char const *const text_)
{
	return text_;
}

char const *getTextFromMap(char const *const text_)
{
	if (!s_translations_ptr)
		return text_;

	auto it = s_translations_ptr->find(text_);
	if (it != std::end(*s_translations_ptr) && !it->first.empty() && !it->second.empty())
		return it->second.c_str();

	return text_;
}
