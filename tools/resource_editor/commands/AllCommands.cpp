/**
 * @file AllCommands.cpp
 * @brief Includes all command headers to trigger REGISTER_COMMAND static initializers.
 *
 * Each command header contains a REGISTER_COMMAND(XxxCommand) invocation that
 * creates a file-scope static registrar object.  By including all headers in
 * a single translation unit we guarantee that every command self-registers
 * before main() runs.
 */

// Full definitions needed by inline command implementations
#include "../CanvasRegistry.h"
#include "../CommandSystem.h"
#include "../FileOperations.h"

// Global commands
#include "global/CreateCanvasCommand.h"
#include "global/DeleteCanvasCommand.h"
#include "global/ExitCommand.h"
#include "global/ExportCommand.h"
#include "global/GridCommand.h"
#include "global/HelpCommand.h"
#include "global/HistoryCommand.h"
#include "global/ListCommand.h"
#include "global/LoadCommand.h"
#include "global/RenameCanvasCommand.h"
#include "global/SaveCommand.h"
#include "global/SelectCommand.h"
#include "global/SetColorCommand.h"
#include "global/SetSizeCommand.h"
#include "global/SetToolCommand.h"
#include "global/ZoomCommand.h"

// Canvas commands
#include "canvas/ClearCommand.h"
#include "canvas/CropCommand.h"
#include "canvas/DrawCircleCommand.h"
#include "canvas/DrawEllipseCommand.h"
#include "canvas/DrawLineCommand.h"
#include "canvas/DrawRectCommand.h"
#include "canvas/FillCommand.h"
#include "canvas/FlipCommand.h"
#include "canvas/FloodFillCommand.h"
#include "canvas/RedoCommand.h"
#include "canvas/ResizeCommand.h"
#include "canvas/RotateCommand.h"
#include "canvas/SetCommand.h"
#include "canvas/UndoCommand.h"
