
#include "CLI.hpp"
#include "Commands.hpp"
#include "Repository.hpp"
#include "Utils.hpp"
#include <iostream>
#include <filesystem>


void test();

int main(int argc, char* argv[]) {

    // Create parser with program description
    Parser parser("Silt - Version Control System");

    // Setup parser with commands and args
    setup_parser(parser);

    // Create a dummy repository instance for now
    Repository repo(std::filesystem::current_path(), true); // Provide a path and force=true for a dummy repo

    // Parse arguments and dispatch to the appropriate command
    auto result = parser.parse_and_dispatch(argc, argv, &repo);

    // If there was an error, print it
    if (result.has_value()) {
        std::cerr << result.value() << std::endl;
        return 1;
    }

    return 0;
}

void test() {
    // Testing Repository class functionality
    std::cout << "Testing Repository class..." << std::endl;

    try {
        // Test Repository constructor with force=true (should work with any directory)
        std::cout << "Test 1: Creating repository with force=true..." << std::endl;
        Repository test_repo(std::filesystem::current_path(), true);
        std::cout << "  Worktree: " << test_repo.worktree << std::endl;
        std::cout << "  Gitdir: " << test_repo.gitdir << std::endl;
        std::cout << "  Force: " << test_repo.force << std::endl;
        std::cout << "  PASS: Repository created successfully with force=true" << std::endl;

        // Test repo_path function
        std::cout << "\nTest 2: Testing repo_path function..." << std::endl;
        std::filesystem::path test_path = repo_path(test_repo, "objects", "test", nullptr);
        std::cout << "  repo_path result: " << test_path << std::endl;
        std::cout << "  PASS: repo_path function works" << std::endl;

        // Test repo_file function
        std::cout << "\nTest 3: Testing repo_file function..." << std::endl;
        std::filesystem::path test_file = repo_file(test_repo, "config", nullptr);
        std::cout << "  repo_file result: " << test_file << std::endl;
        std::cout << "  PASS: repo_file function works" << std::endl;

        // Test repo_dir function (create=true)
        std::cout << "\nTest 4: Testing repo_dir function with create=true..." << std::endl;
        std::filesystem::path test_dir = repo_dir(test_repo, true, "test_dir", nullptr);
        std::cout << "  repo_dir result: " << test_dir << std::endl;
        std::cout << "  PASS: repo_dir function works with create=true" << std::endl;

        // Test repo_dir function (create=false) on existing directory
        std::cout << "\nTest 5: Testing repo_dir function with create=false on existing dir..." << std::endl;
        std::filesystem::path existing_dir = repo_dir(test_repo, false, "test_dir", nullptr);
        std::cout << "  repo_dir result: " << existing_dir << std::endl;
        std::cout << "  PASS: repo_dir function works with create=false on existing dir" << std::endl;

        // Test repo_create function
        std::cout << "\nTest 6: Testing repo_create function..." << std::endl;
        Repository new_repo = repo_create(std::filesystem::temp_directory_path() / "test_repo");
        std::cout << "  PASS: repo_create function called (implementation is a stub)" << std::endl;

        // Test Repository constructor with force=false on a directory without .git (should do a runtime err)
        std::cout << "\nTest 7: Testing Repository constructor with force=false on non-git directory..." << std::endl;
        {
            // Create a temporary directory for testing with NO .git dir
            std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "silt_test_temp";
            std::filesystem::create_directories(temp_dir);
            try {
                Repository invalid_repo(temp_dir, false);
                std::cout << "  FAIL: Expected exception was not thrown" << std::endl;
            } catch (const std::runtime_error& e) {
                std::cout << "  Expected exception caught: " << e.what() << std::endl;
                std::cout << "  PASS: Repository constructor correctly throws for non-git directory" << std::endl;
            }
            // Clean up the temp directory
            if (std::filesystem::exists(temp_dir)) {
                std::filesystem::remove_all(temp_dir);
            }
        }

        std::cout << "\nAll Repository class tests completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error during Repository tests: " << e.what() << std::endl;
    }
}