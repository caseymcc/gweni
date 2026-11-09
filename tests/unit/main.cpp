/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  doctest main for gweni_unit_tests, the headless unit tests: no GL context,
 *  no resources, so they run anywhere (ctest -LE gpu).
 *
 *  A run that matches no test case fails. doctest_discover_tests registers
 *  each case as the CTest test unit:<name>, running --test-case=<name>, and
 *  turns the case list into a CMake list, so a ';' in a case name splits it
 *  into two tests whose filters match nothing; doctest exits 0 for that, and
 *  the case never ran under ctest while both halves passed. So when a run
 *  (not a query: --list-test-cases, --count, --help, --version, ...) passes
 *  no case through its filters, main prints one line on stderr and exits 1.
 *  unit_tests_zero_match_fails (tests/CMakeLists.txt) pins it.
 */
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <cstdio>

namespace
{

// Set by test_run_end, which doctest reports for a run that executes the
// filtered cases and never for a query.
bool g_runEnded=false;
unsigned g_casesPassingFilters=0;

struct ZeroMatchListener: doctest::IReporter
{
    explicit ZeroMatchListener(const doctest::ContextOptions &) {}

    void report_query(const doctest::QueryData &) override {}
    void test_run_start() override {}
    void test_run_end(const doctest::TestRunStats &stats) override
    {
        g_runEnded=true;
        g_casesPassingFilters=stats.numTestCasesPassingFilters;
    }
    void test_case_start(const doctest::TestCaseData &) override {}
    void test_case_reenter(const doctest::TestCaseData &) override {}
    void test_case_end(const doctest::CurrentTestCaseStats &) override {}
    void test_case_exception(const doctest::TestCaseException &) override {}
    void subcase_start(const doctest::SubcaseSignature &) override {}
    void subcase_end() override {}
    void log_assert(const doctest::AssertData &) override {}
    void log_message(const doctest::MessageData &) override {}
    void test_case_skipped(const doctest::TestCaseData &) override {}
};

REGISTER_LISTENER("gweni_zero_match", 1, ZeroMatchListener);

}//namespace

int main(int argc, char **argv)
{
    doctest::Context context(argc, argv);
    const int result=context.run();

    if(context.shouldExit() || !g_runEnded || result != 0 || g_casesPassingFilters != 0)
        return result;

    std::fprintf(stderr, "gweni_unit_tests: no test case matched the filter "
        "(a ';' in a case name splits its CTest registration by doctest_discover_tests)\n");
    return 1;
}
