#include <iostream>
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

ErrorType redrawLine(const int row, const int col, const std::string &text, const int consoleWidth = 120)
{
	// Hide cursor to prevent flickering
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

	// Position cursor in the beginning of the line
	COORD pos = {0, static_cast<SHORT>(row)};
	if (!SetConsoleCursorPosition(hConsole, pos))
	{
		return ErrorType::SetConsoleCursorPositionError;
	}

	// Clear line by writing blank characters
	const std::string spaces(consoleWidth, ' ');
	DWORD written;
	WriteConsole(hConsole, spaces.c_str(), spaces.size(), &written, nullptr);

	// Reset cursor to the beginning and write new line of text
	if (!SetConsoleCursorPosition(hConsole, pos))
	{
		return ErrorType::SetConsoleCursorPositionError;
	}
	WriteConsole(hConsole, text.c_str(), text.size(), &written, nullptr);

	// Set cursor position to the end of text
	// pos.X = static_cast<SHORT>(text.length());
	pos.X = static_cast<SHORT>(col);
	if (!SetConsoleCursorPosition(hConsole, pos))
	{
		return ErrorType::SetConsoleCursorPositionError;
	}

	// Show cursor to prevent flickering
	cursorInfo.bVisible = true;
	if (!SetConsoleCursorInfo(hConsole, &cursorInfo))
	{
		return ErrorType::GetConsoleCursorInfoError;
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
		if (const ErrorType error = redrawLine(row, col, current_line); error != ErrorType::None)
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
				}
				col = 0;
			}
			else if (key_code == VK_DOWN)
			{
				if (row < maxRow)
				{
					++row;
				}
				col = 0;
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

				col = 0;

				// If we're at the last line, add an empty line to the end
				if (row == maxRow)
				{
					buffer.emplace_back();
				}
				else
				{
					// Insert a new line in the middle and redraw everything below that line
					buffer.emplace(buffer.begin() + row);

					for (int i = 0; i < buffer.size(); ++i)
					{
						const auto &line = buffer[i];
						redrawLine(i, line.length(), line);
					}
				}
			}
			else if (key_code == VK_BACK)
			{
				if (!current_line.empty())
				{
					if (col > 0)
					{
						current_line.erase(col - 1, 1);
						--col;
					}
					else if (row > 0)
					{
						--row;
					}
				}
				// If line is empty, delete it from buffer and redraw
				else if (row > 0)
				{
					buffer.erase(buffer.begin() + row);
					--row;

					system("cls");
					renderBuffer(buffer);
				}
			}
			// 0 - 9, a-z, spacebar, miscellaneous characters
			else if ((key_code >= 0x30 && key_code <= 0x39) || (key_code >= 0x41 && key_code <= 0x5A) || key_code ==
				VK_SPACE || (key_code >= 0xBA && key_code <= 0xC0) || (key_code >= 0xDB && key_code <= 0xDF) || key_code
				== 0xE2)
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
