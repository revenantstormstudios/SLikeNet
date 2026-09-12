/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// A deliberately tiny, dependency-free test harness.
//
// These tests exist primarily as regression tests for security fixes: most of them feed a
// hand-crafted BitStream (i.e. what a malicious peer could put on the wire) straight into a
// deserializer and assert that it rejects the input rather than reading or writing out of
// bounds. They do not open sockets and do not require any external service.
//
// Build them with ASan enabled to make an out-of-bounds access a hard failure rather than
// silent corruption - see .github/workflows/ci.yml.

#ifndef __SLIKENET_TEST_HARNESS_H
#define __SLIKENET_TEST_HARNESS_H

#include <vector>

namespace slnTest
{
	typedef void (*TestFn)();

	struct TestCase
	{
		const char *name;
		TestFn fn;
	};

	std::vector<TestCase>& Registry();

	// Records a failure against the currently running test and prints file/line/expression.
	void ReportFailure(const char *file, int line, const char *expression, const char *detail);

	// Runs every registered test. Returns the number of failed tests.
	int RunAll();

	struct Registrar
	{
		Registrar(const char *name, TestFn fn) { Registry().push_back(TestCase{ name, fn }); }
	};
}

/// Defines and self-registers a test case.
#define SLN_TEST(testName)                                                        \
	static void testName();                                                       \
	static slnTest::Registrar slnTestRegistrar_##testName(#testName, testName);   \
	static void testName()

/// Fails the current test (and continues) if \a expression is false.
#define SLN_CHECK(expression)                                                     \
	do {                                                                          \
		if (!(expression))                                                        \
			slnTest::ReportFailure(__FILE__, __LINE__, #expression, nullptr);      \
	} while (0)

/// As SLN_CHECK, but attaches a short explanation to the failure output.
#define SLN_CHECK_MSG(expression, detail)                                         \
	do {                                                                          \
		if (!(expression))                                                        \
			slnTest::ReportFailure(__FILE__, __LINE__, #expression, detail);       \
	} while (0)

#endif // __SLIKENET_TEST_HARNESS_H
