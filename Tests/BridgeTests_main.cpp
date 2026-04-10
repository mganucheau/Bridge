#include <JuceHeader.h>
#include <iostream>

// Tests in other translation units self-register via static UnitTest instances.

int main (int argc, char* argv[])
{
    juce::ignoreUnused (argc, argv);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (false);
    runner.runAllTests();

    int totalFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            totalFailures += r->failures;

    if (totalFailures > 0)
        std::cerr << "Bridge QA: " << totalFailures << " assertion(s) failed.\n";

    return totalFailures > 0 ? 1 : 0;
}
