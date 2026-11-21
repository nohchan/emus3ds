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
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "3dsgettext.h"

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
	using StringMap = std::map<std::string, std::string>;
	using Table     = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

	StringMap s_translations;

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
		char path[64];
		std::sprintf(path, "romfs:/locale/%s/emus3ds.mo", language_);

		FILE *file = fopen(path, "rb");
		if (file == NULL) {
			// romfsに.moファイルが無い場合でも主要UIを英語化する
			s_translations["选项"] = "Options";
			s_translations["选择 ROM"] = "Select ROM";
			s_translations["选择ROM"] = "Select ROM";
			s_translations["  语言"] = "  Language";
			s_translations["选择应用程序显示的语言"] = "Select the language for the UI";
			s_translations["  主题"] = "  Theme";
			s_translations["选择应用于界面的主题"] = "Select the theme for the UI";
			s_translations["  字体"] = "  Font";
			s_translations["用于用户界面的字体(仅适用于ASCII字符)"] = "Font for UI (ASCII characters only)";
			s_translations["  退出"] = "  Exit";
			s_translations["退出"] = "Exit";
			s_translations["立即退出?"] = "Exit now?";
			s_translations["原始"] = "Original";
			s_translations["暗黑模式"] = "Dark Mode";
			s_translations["否"] = "No";
			s_translations["是"] = "Yes";
			s_translations["确定"] = "OK";
			s_translations["A:选择  B:取消  X+上/下:翻页"] = "A:Select B:Back X:Page";
			s_translations["模拟器"] = "Emulator";
			s_translations["设置"] = "Settings";
			s_translations["控制"] = "Controls";
			s_translations["金手指"] = "Cheats";
			s_translations["加载 ROM"] = "Load ROM";
			s_translations["加载中... 请稍后."] = "Loading... Please wait.";
			s_translations["无法加载 ROM."] = "Unable to load ROM.";
			s_translations["保存设置到SD卡..."] = "Saving settings to SD card...";
			s_translations["即时存档"] = "Save State";
			s_translations["自动保存..."] = "Auto-saving...";
			s_translations["自动保存失败"] = "Auto-save failed";
			s_translations["自动保存失败.\n强制加载?"] = "Auto-save failed.\nForce load?";
			s_translations["截屏"] = "Screenshot";
			s_translations["开始截屏...\n请稍后."] = "Taking screenshot...\nPlease wait.";
			s_translations["截屏时发生错误!"] = "Error taking screenshot!";
			s_translations["重置控制台"] = "Reset Console";
			s_translations["确定吗?"] = "Are you sure?";
			s_translations["无此游戏可用的金手指."] = "No cheats available for this game.";
			s_translations["此游戏没有可用的金手指."] = "No cheats available for this game.";
			return;
		}


		std::uint32_t magic;
		if (!read(file, magic, true))
			return;

		bool le;
		if (magic == 0x950412DE)
			le = true;
		else if (magic == 0xDE120495)
			le = false;
		else
			return;

		std::uint32_t version;
		if (!read(file, version, le) || version != 0)
			return;

		std::uint32_t numStrings;
		if (!read(file, numStrings, le))
			return;

		std::uint32_t originalOffset;
		if (!read(file, originalOffset, le))
			return;

		std::uint32_t translationOffset;
		if (!read(file, translationOffset, le))
			return;

		std::uint32_t hashSize;
		if (!read(file, hashSize, le))
			return;

		std::uint32_t hashOffset;
		if (!read(file, hashOffset, le))
			return;

		auto originalTable = loadTable(file, originalOffset, numStrings, le);
		if (originalTable.size () != numStrings)
			return;

		auto translationTable = loadTable(file, translationOffset, numStrings, le);
		if (translationTable.size () != numStrings)
			return;

		StringMap strings;
		for (std::uint32_t i = 0; i < numStrings; ++i)
		{
			auto originalString = loadString(file, originalTable[i].first, originalTable[i].second);
			auto translationString =
				loadString(file, translationTable[i].first, translationTable[i].second);

			strings.emplace(std::make_pair(originalString, translationString));
		}

		std::swap(strings, s_translations);
	}
}

void setLanguage(char const *const language_)
{
	if (language_ == "zh_Hans")
	{
		if (!s_translations.empty())
			s_translations.clear();
	} else
		loadTranslations(language_);
}

char const *getText(char const *const text_)
{
	return text_;
}

char const *getTextFromMap(char const *const text_)
{
	auto it = s_translations.find(text_);
	if (it != std::end(s_translations) && !it->first.empty() && !it->second.empty())
		return it->second.c_str();

	return text_;
}
