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

ErrorType redrawLine(const int row, const std::string &text, const int consoleWidth = 120)
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
	pos.X = static_cast<SHORT>(text.length());
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
	std::vector<std::string> buffer;

	// renderBuffer(buffer);

	// Input loop
	INPUT_RECORD inputRecord;
	DWORD nrOfEventsRead;

	std::string line;
	SHORT row = 0, col = 0;

	while (true)
	{
		// Draw line
		if (const ErrorType error = redrawLine(row, line); error != ErrorType::None)
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

			// ESC key
			if (key_code == VK_ESCAPE)
			{
				break;
			}

			// Enter key
			if (key_code == VK_RETURN)
			{
				buffer.push_back(line);
				line.clear();
				++row;
			}
			// Backspace
			else if (key_code == VK_BACK)
			{
				if (!line.empty())
				{
					line.pop_back();

					if (col > 0)
					{
						--col;
					}
				}
				else if (row > 0)
				{
					--row;
				}
			}
			// 0 - 9 and A - Z keys
			else if ((key_code >= 0x30 && key_code <= 0x39) || (key_code >= 0x41 && key_code <= 0x5A))
			{
				line += inputRecord.Event.KeyEvent.uChar.AsciiChar;
				++col;
			}
		}
	}

	restoreConsole();
	return 0;
}
