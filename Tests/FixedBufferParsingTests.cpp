/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// Several fixed-buffer parsers bounded their fill loop by the buffer size and then wrote a
// terminator at the resulting index, which lands one byte past the end when the input is long
// enough to fill the buffer exactly.
//
// Run under AddressSanitizer (SLIKENET_ENABLE_ASAN=ON) for these to be meaningful.

#include "TestHarness.h"

#include "slikenet/CommandParserInterface.h"
#include "slikenet/types.h"

#include <cstring>

SLN_TEST(SystemAddress_OverlongNumericHostDoesNotOverflow)
{
	// The IP part buffer is 22 bytes and the loop used "index < 22", so 22 digits drove the
	// terminator to IPPart[22].
	SLNet::SystemAddress address;
	address.FromString("1111111111111111111111");

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");
}

SLN_TEST(SystemAddress_OverlongPortDoesNotOverflow)
{
	// The port buffer is 10 bytes with a "portIndex < 10" bound, so 10 port digits drove the
	// terminator to portPart[10].
	SLNet::SystemAddress address;
	address.FromString("1.2.3.4|0000000000");

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");
}

SLN_TEST(ParseConsoleString_FullParameterListDoesNotOverflow)
{
	// ParseConsoleString writes a NULL terminator one slot past the last parameter, so a caller
	// passing an array of exactly parameterListLength - which ConsoleServer::Update does - had
	// parameterList[20] written when the input carried 20 tokens.
	char input[] = "a b c d e f g h i j k l m n o p q r s t";
	char *parameterList[20];
	unsigned numParameters = 0;

	SLNet::CommandParserInterface::ParseConsoleString(input, ' ', '"', &numParameters, parameterList, 20);

	SLN_CHECK_MSG(numParameters < 20, "the terminator slot must be reserved within the array");
	SLN_CHECK(parameterList[numParameters] == 0);
}

SLN_TEST(ParseConsoleString_NormalInputStillParses)
{
	// Guards against the reserved slot breaking ordinary commands.
	char input[] = "help connect 127.0.0.1";
	char *parameterList[20];
	unsigned numParameters = 0;

	SLNet::CommandParserInterface::ParseConsoleString(input, ' ', '"', &numParameters, parameterList, 20);

	SLN_CHECK(numParameters == 3);
	SLN_CHECK(strcmp(parameterList[0], "help") == 0);
	SLN_CHECK(strcmp(parameterList[1], "connect") == 0);
	SLN_CHECK(strcmp(parameterList[2], "127.0.0.1") == 0);
}
