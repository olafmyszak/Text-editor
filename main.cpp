#include <filesystem>
#include <fstream>
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
	CommandLineArgumentsError,
	FileOpenError,
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
		case ErrorType::CommandLineArgumentsError: return os << "CommandLineArgumentsError";
		case ErrorType::FileOpenError: return os << "FileOpenError";
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
	// or specific column
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

ErrorType readFile(const std::string &path, std::vector<std::string> &buffer)
{
	std::ifstream inf{path};

	if (!inf)
	{
		return ErrorType::FileOpenError;
	}

	std::vector<std::string> tmp_buffer;

	std::string line;
	while (std::getline(inf, line))
	{
		tmp_buffer.push_back(line);
	}

	buffer = tmp_buffer;

	return ErrorType::None;
}

ErrorType readFile(const std::filesystem::path &path, std::vector<std::string> &buffer)
{
	std::ifstream inf{path};

	if (!inf)
	{
		return ErrorType::FileOpenError;
	}

	std::vector<std::string> tmp_buffer;

	std::string line;
	while (std::getline(inf, line))
	{
		tmp_buffer.push_back(line);
	}

	buffer = tmp_buffer;

	return ErrorType::None;
}

ErrorType saveFile(const std::string &path, const std::vector<std::string> &buffer)
{
	std::ofstream outf{path};

	if (!outf)
	{
		return ErrorType::FileOpenError;
	}

	for (const auto &line : buffer)
	{
		outf << line << '\n';
	}

	return ErrorType::None;
}

ErrorType saveFile(const std::filesystem::path &path, const std::vector<std::string> &buffer)
{
	std::ofstream outf{path};

	if (!outf)
	{
		return ErrorType::FileOpenError;
	}

	for (const auto &line : buffer)
	{
		outf << line << '\n';
	}

	return ErrorType::None;
}

bool askForConfirmation(const std::string &message)
{
	std::string input;
	while (true)
	{
		std::cout << message << " (y/n): ";
		std::getline(std::cin, input);

		// Convert input to lowercase for case-insensitivity
		if (!input.empty())
		{
			const char response = static_cast<char>(std::tolower(input[0]));

			if (response == 'y')
			{
				return true;
			}

			if (response == 'n')
			{
				return false;
			}
		}

		// If input is invalid, prompt again
		std::cout << "Invalid input. Please enter 'y' for yes or 'n' for no.\n";
	}
}

int main(int argc, char *argv[])
{
	if (argc > 2)
	{
		std::cout << "Usage: " << argv[0] << " [file]\n";
		return static_cast<int>(ErrorType::CommandLineArgumentsError);
	}

	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE)
	{
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

	std::vector<std::string> buffer{1};

	if (argc == 2)
	{
		const std::string filename = argv[1];
		const std::filesystem::path path = std::filesystem::current_path() / filename;

		if (const ErrorType error = readFile(path, buffer); error != ErrorType::None)
		{
			std::cerr << error << '\n';
			restoreConsole();
			return static_cast<int>(error);
		}

		renderBuffer(buffer);
	}

	// Input loop
	INPUT_RECORD inputRecord;
	DWORD nrOfEventsRead;

	int current_row = 0, current_col = 0;

	while (true)
	{
		std::string &current_line = buffer.at(current_row);

		// Draw line
		if (const ErrorType error = redrawLine(current_row, current_line, consoleWidth, current_col); error != ErrorType::None)
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

			//TODO: better cursor movement
			if (key_code == VK_UP)
			{
				if (current_row > 0)
				{
					--current_row;
					current_col = static_cast<int>(buffer[current_row].length());
				}
			}
			else if (key_code == VK_DOWN)
			{
				if (current_row < buffer.size() - 1)
				{
					++current_row;
					current_col = static_cast<int>(buffer[current_row].length());
				}
			}
			else if (key_code == VK_LEFT)
			{
				if (current_col > 0)
				{
					--current_col;
				}
			}
			else if (key_code == VK_RIGHT)
			{
				if (current_col < current_line.length())
				{
					++current_col;
				}
			}
			else if (key_code == VK_RETURN)
			{
				++current_row;

				// If the cursor is in the middle of a line, split the line at the cursor position and move the text after the cursor to a new line below
				if (current_col < current_line.length())
				{
					const std::string firstPart = current_line.substr(0, current_col);
					const std::string secondPart = current_line.substr(current_col);

					buffer[current_row - 1] = firstPart;
					buffer.insert(buffer.begin() + current_row, secondPart);
				}
				else
				{
					if (current_row == buffer.size() - 1)
					{
						buffer.emplace_back();
					}
					else
					{
						buffer.emplace(buffer.begin() + current_row);
					}
				}

				// Redraw modified lines
				for (int i = current_row - 1; i < buffer.size(); ++i)
				{
					const auto &line = buffer[i];
					redrawLine(i, line, consoleWidth);
				}

				current_col = 0;
			}
			else if (key_code == VK_BACK)
			{
				// If cursor is in the beginning of line, merge upper and current line and move the rest one line up
				if (current_col == 0)
				{
					if (current_row > 0)
					{
						auto old_upper_line_length = buffer[current_row - 1].length();
						buffer[current_row - 1] += current_line;

						buffer.erase(buffer.begin() + current_row);
						--current_row;

						// Redraw lines after the deleted one
						for (int i = current_row; i < buffer.size(); ++i)
						{
							const auto &line = buffer[i];
							redrawLine(i, line, consoleWidth);
						}

						// and clear the last one
						clearLine(static_cast<int>(buffer.size() - 1), consoleWidth);

						// Move the cursor to the end of upper line before merging
						current_col = static_cast<int>(old_upper_line_length);
					}
				}
				else
				{
					current_line.erase(current_col - 1, 1);
					--current_col;
				}
			}
			else if (key_code == VK_TAB)
			{
				// Insert 4 spaces
				current_line.insert(current_col, "    ");
				current_col += 4;

				redrawLine(current_row, current_line, consoleWidth, current_col);
			}
			else if (printableKeyCodes.contains(key_code))
			{
				// Insert a new character after cursor
				current_line.insert(current_col, 1, inputRecord.Event.KeyEvent.uChar.AsciiChar);
				++current_col;
			}
		}
	}

	system("cls");
	if (askForConfirmation("Save modified buffer?"))
	{
		std::string filename;

		// If a file was opened, save the modified contents
		if (argc == 2)
		{
			filename = argv[1];
		}
		// else ask for a filename to write
		else
		{
			std::cout << "Filename to write: ";
			std::getline(std::cin, filename);
		}

		const std::filesystem::path path = std::filesystem::current_path() / filename;

		if (const ErrorType error = saveFile(path, buffer); error != ErrorType::None)
		{
			std::cerr << error << '\n';
			restoreConsole();
			return static_cast<int>(error);
		}
	}

	restoreConsole();

	return 0;
}
