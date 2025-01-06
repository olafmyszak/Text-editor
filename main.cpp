#include <iostream>
#include <sstream>
#include <unordered_set>
#include <windows.h>
#include <vector>

HANDLE hStdin;
HANDLE hConsole;
DWORD fdwSaveOldMode;

enum class ErrorType
{
	None,
	GetConsoleModeError,
	SetConsoleModeError,
	SetConsoleCursorPositionError,
	GetStdHandleError,
	ReadConsoleInputError,
	GetConsoleScreenBufferInfoError,
	GetConsoleCursorInfoError,
	SetConsoleCursorInfoError,
};

std::ostream &operator<<(std::ostream &os, const ErrorType errorType)
{
	switch (errorType)
	{
		case ErrorType::None: return os << "No Error";
		case ErrorType::GetConsoleModeError: return os << "GetConsoleMode";
		case ErrorType::SetConsoleModeError: return os << "SetConsoleMode";
		case ErrorType::SetConsoleCursorPositionError: return os << "SetConsoleCursorPosition";
		case ErrorType::GetStdHandleError: return os << "GetStdHandle";
		case ErrorType::ReadConsoleInputError: return os << "ReadConsoleInput";
		case ErrorType::GetConsoleScreenBufferInfoError: return os << "GetConsoleScreenBufferInfo";
		case ErrorType::GetConsoleCursorInfoError: return os << "GetConsoleCursorInfoError";
		case ErrorType::SetConsoleCursorInfoError: return os << "SetConsoleCursorInfoError";
	}

	return os;
}

std::string coordToString(const COORD coord)
{
	std::ostringstream oss;
	oss << "{" << coord.X << ", " << coord.Y << "}";
	return oss.str();
}

ErrorType setupConsole()
{
	// Save the current input mode, to be restored on exit
	if (!GetConsoleMode(hStdin, &fdwSaveOldMode))
	{
		return ErrorType::GetConsoleModeError;
	}

	DWORD fdwMode = fdwSaveOldMode;
	fdwMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT); // Disable line buffering and echo

	if (!SetConsoleMode(hStdin, fdwMode))
	{
		return ErrorType::SetConsoleModeError;
	}

	system("cls"); // Clear screen

	return ErrorType::None;
}

void restoreConsole()
{
	SetConsoleMode(hStdin, fdwSaveOldMode);
}

ErrorType renderBuffer(const std::vector<std::string> &buffer)
{
	COORD position = {0, 0};
	if (!SetConsoleCursorPosition(hConsole, position))
	{
		return ErrorType::SetConsoleCursorPositionError;
	}

	for (const auto &line : buffer)
	{
		std::cout << line << "\n";
	}

	return ErrorType::None;
}

ErrorType hideCursor()
{
	CONSOLE_CURSOR_INFO cursorInfo;
	if (!GetConsoleCursorInfo(hConsole, &cursorInfo))
	{
		return ErrorType::GetConsoleCursorInfoError;
	}
	cursorInfo.bVisible = false;
	if (!SetConsoleCursorInfo(hConsole, &cursorInfo))
	{
		return ErrorType::GetConsoleCursorInfoError;
	}
	return ErrorType::None;
}

ErrorType showCursor()
{
	CONSOLE_CURSOR_INFO cursorInfo;
	if (!GetConsoleCursorInfo(hConsole, &cursorInfo))
	{
		return ErrorType::GetConsoleCursorInfoError;
	}
	cursorInfo.bVisible = true;
	if (!SetConsoleCursorInfo(hConsole, &cursorInfo))
	{
		return ErrorType::GetConsoleCursorInfoError;
	}
	return ErrorType::None;
}

ErrorType clearLine(const int row, const int consoleWidth)
{
	// Position cursor in the beginning of the line
	if (COORD pos = {0, static_cast<SHORT>(row)}; !SetConsoleCursorPosition(hConsole, pos))
	{
		return ErrorType::SetConsoleCursorPositionError;
	}

	// Clear line by writing blank characters
	const std::string spaces(consoleWidth, ' ');
	DWORD written;
	WriteConsole(hConsole, spaces.c_str(), spaces.size(), &written, nullptr);

	return ErrorType::None;
}

ErrorType writeLine(const int row, const std::string &text, const int col = -1)
{
	COORD pos = {0, static_cast<SHORT>(row)};
	// Reset cursor to the beginning and write new line of text
	if (!SetConsoleCursorPosition(hConsole, pos))
	{
		return ErrorType::SetConsoleCursorPositionError;
	}

	DWORD written;
	WriteConsole(hConsole, text.c_str(), text.size(), &written, nullptr);

	// Set cursor position to the end of text
	if (col == -1)
	{
		pos.X = static_cast<SHORT>(text.length());
	}
	// or specifc column
	else
	{
		pos.X = static_cast<SHORT>(col);
	}

	if (!SetConsoleCursorPosition(hConsole, pos))
	{
		return ErrorType::SetConsoleCursorPositionError;
	}

	return ErrorType::None;
}

ErrorType redrawLine(const int row, const std::string &text, const int consoleWidth, const int col = -1)
{
	// Hide cursor to prevent flickering
	if (const ErrorType error = hideCursor(); error != ErrorType::None)
	{
		return error;
	}

	if (const ErrorType error = clearLine(row, consoleWidth); error != ErrorType::None)
	{
		return error;
	}

	if (const ErrorType error = writeLine(row, text, col); error != ErrorType::None)
	{
		return error;
	}

	if (const ErrorType error = showCursor(); error != ErrorType::None)
	{
		return error;
	}

	return ErrorType::None;
}

void logBufferToDebugOutput(const std::vector<std::string> &buffer)
{
	std::string message = "\n------------------\n";

	for (const auto &line : buffer)
	{
		message += line;
		message += "\n";
	}

	OutputDebugStringA(message.c_str());
}

const std::unordered_set printableKeyCodes = {
	VK_SPACE, 0xE2, // Individual keys
	// Numeric keys
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	// Alphabetic keys
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
	// Special keys
	0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF
};

int main()
{
	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE)
	{
		// std::cerr << "GetStdHandle\n";
		std::cerr << ErrorType::GetStdHandleError << '\n';
		return static_cast<int>(ErrorType::GetStdHandleError);
	}

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == INVALID_HANDLE_VALUE)
	{
		std::cerr << ErrorType::GetStdHandleError << '\n';
		return static_cast<int>(ErrorType::GetStdHandleError);
	}

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	const int consoleWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;

	if (const ErrorType error = setupConsole(); error != ErrorType::None)
	{
		std::cerr << error << '\n';
		restoreConsole();
		return static_cast<int>(error);
	}

	// std::vector<std::string> buffer = {"Welcome to My Text Editor!", "Press ESC to exit."};
	std::vector<std::string> buffer{1};

	// Input loop
	INPUT_RECORD inputRecord;
	DWORD nrOfEventsRead;

	int row = 0, col = 0, maxRow = 0;

	while (true)
	{
		std::string &current_line = buffer.at(row);

		// Draw line
		if (const ErrorType error = redrawLine(row, current_line, consoleWidth, col); error != ErrorType::None)
		{
			std::cerr << error << '\n';
			restoreConsole();
			return static_cast<int>(error);
		}

		// Read next event
		if (!ReadConsoleInput(hStdin, &inputRecord, 1, &nrOfEventsRead))
		{
			std::cerr << ErrorType::ReadConsoleInputError << '\n';
			restoreConsole();
			return static_cast<int>(ErrorType::ReadConsoleInputError);
		}

		// Process line
		if (inputRecord.EventType == KEY_EVENT && inputRecord.Event.KeyEvent.bKeyDown)
		{
			const auto key_code = inputRecord.Event.KeyEvent.wVirtualKeyCode;

			if (key_code == VK_ESCAPE)
			{
				break;
			}

			if (key_code == VK_UP)
			{
				if (row > 0)
				{
					--row;
					col = static_cast<int>(buffer[row].length());
				}
			}
			else if (key_code == VK_DOWN)
			{
				if (row < maxRow)
				{
					++row;
					col = static_cast<int>(buffer[row].length());
				}
			}
			else if (key_code == VK_LEFT)
			{
				if (col > 0)
				{
					--col;
				}
			}
			else if (key_code == VK_RIGHT)
			{
				if (col < current_line.length())
				{
					++col;
				}
			}
			else if (key_code == VK_RETURN)
			{
				++row;
				++maxRow;

				// If the cursor is in the middle of a line, split the line at the cursor position and move the text after the cursor to a new line below
				if (col < current_line.length())
				{
					const std::string firstPart = current_line.substr(0, col);
					const std::string secondPart = current_line.substr(col);

					buffer[row - 1] = firstPart;
					buffer.insert(buffer.begin() + row, secondPart);
				}
				else
				{
					if (row == maxRow)
					{
						buffer.emplace_back();
					}
					else
					{
						buffer.emplace(buffer.begin() + row);
					}
				}

				// Redraw modified lines
				for (int i = row - 1; i <= maxRow; ++i)
				{
					const auto &line = buffer[i];
					redrawLine(i, line, consoleWidth);
				}

				col = 0;
			}
			else if (key_code == VK_BACK)
			{
				// If cursor is in the beginning of line, merge upper and current line and move the rest one line up
				if (col == 0)
				{
					if (row > 0)
					{
						auto old_upper_line_length = buffer[row - 1].length();
						buffer[row - 1] += current_line;

						buffer.erase(buffer.begin() + row);
						--row;
						--maxRow;

						// Redraw lines after the deleted one
						for (int i = row; i <= maxRow; ++i)
						{
							const auto &line = buffer[i];
							redrawLine(i, line, consoleWidth);
						}

						// and clear the last one
						clearLine(maxRow + 1, consoleWidth);

						// Move the cursor to the end of upper line before merging
						col = static_cast<int>(old_upper_line_length);
					}
				}
				else
				{
					current_line.erase(col - 1, 1);
					--col;
				}
			}
			else if (printableKeyCodes.contains(key_code))
			{
				// Insert a new character after cursor
				// current_line += inputRecord.Event.KeyEvent.uChar.AsciiChar;
				current_line.insert(col, 1, inputRecord.Event.KeyEvent.uChar.AsciiChar);
				++col;
			}
		}
	}

	restoreConsole();

	return 0;
}
