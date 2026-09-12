/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/
#include "TestHarness.h"

#include <cstdio>

namespace
{
	int s_currentTestFailures = 0;
}

namespace slnTest
{
	std::vector<TestCase>& Registry()
	{
		// Function-local static: avoids any dependency on static initialization order between
		// the registrars in the individual test translation units.
		static std::vector<TestCase> registry;
		return registry;
	}

	void ReportFailure(const char *file, int line, const char *expression, const char *detail)
	{
		++s_currentTestFailures;
		if (detail != nullptr)
			std::printf("    FAIL %s(%d): %s\n         %s\n", file, line, expression, detail);
		else
			std::printf("    FAIL %s(%d): %s\n", file, line, expression);
	}

	int RunAll()
	{
		const std::vector<TestCase> &tests = Registry();
		int failedTests = 0;

		std::printf("Running %u test(s)\n", static_cast<unsigned>(tests.size()));

		for (size_t i = 0; i < tests.size(); ++i)
		{
			s_currentTestFailures = 0;
			std::printf("  %s\n", tests[i].name);
			tests[i].fn();

			if (s_currentTestFailures != 0)
				++failedTests;
		}

		if (failedTests == 0)
			std::printf("All %u test(s) passed.\n", static_cast<unsigned>(tests.size()));
		else
			std::printf("%d of %u test(s) FAILED.\n", failedTests, static_cast<unsigned>(tests.size()));

		return failedTests;
	}
}

int main()
{
	// Non-zero exit code fails the CTest run.
	return slnTest::RunAll() == 0 ? 0 : 1;
}
