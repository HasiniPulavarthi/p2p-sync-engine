#include <iostream>
#include <string>
#include <vector>
#include "node.hpp"

namespace {

void print_usage() {
    std::cout <<
        "Usage: syncnode --id <node_id> --data <data_dir> <command> [args]\n\n"
        "Commands:\n"
        "  add <logical_path> <source_file>   Ingest a real file into the sync engine\n"
        "  rm  <logical_path>                 Tombstone (delete) a tracked path\n"
        "  list                                List all tracked entries and their versions\n"
        "  listen --port <port>                Wait for one peer to connect and sync\n"
        "  sync  --host <host> --port <port>   Connect to a peer and sync\n\n"
        "Example (two terminals, simulating two offline-then-reconnecting nodes):\n"
        "  syncnode --id A --data ./dataA add notes.txt ./notes.txt\n"
        "  syncnode --id A --data ./dataA listen --port 9001\n"
        "  syncnode --id B --data ./dataB add notes.txt ./notes_editedB.txt\n"
        "  syncnode --id B --data ./dataB sync --host 127.0.0.1 --port 9001\n";
}

std::string arg_value(const std::vector<std::string>& args, const std::string& flag) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) return args[i + 1];
    }
    return "";
}

void print_stats(const SyncStats& s) {
    std::cout << "--- sync complete ---\n"
              << "  entries sent:            " << s.entries_sent << "\n"
              << "  applied from remote:     " << s.entries_applied_remote << "\n"
              << "  kept local (we're newer):" << s.entries_kept_local << "\n"
              << "  already in sync:         " << s.entries_no_change << "\n"
              << "  conflicts forked:        " << s.conflicts_forked << "\n"
              << "  chunks fetched:          " << s.chunks_fetched << "\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) { print_usage(); return 1; }

    std::string node_id = arg_value(args, "--id");
    std::string data_dir = arg_value(args, "--data");
    if (node_id.empty() || data_dir.empty()) {
        std::cerr << "error: --id and --data are required\n\n";
        print_usage();
        return 1;
    }

    // Find the command: first token that isn't part of a --flag/value pair.
    std::string command;
    std::vector<std::string> rest;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--id" || args[i] == "--data" || args[i] == "--host" || args[i] == "--port") {
            i++; // skip the flag's value too
            continue;
        }
        if (command.empty()) command = args[i];
        else rest.push_back(args[i]);
    }

    try {
        Node node(node_id, data_dir);

        if (command == "add") {
            if (rest.size() < 2) { std::cerr << "usage: add <logical_path> <source_file>\n"; return 1; }
            node.add_file(rest[0], rest[1]);
            std::cout << "added " << rest[0] << "\n";
        } else if (command == "rm") {
            if (rest.empty()) { std::cerr << "usage: rm <logical_path>\n"; return 1; }
            node.remove_file(rest[0]);
            std::cout << "removed " << rest[0] << "\n";
        } else if (command == "list") {
            node.list_files();
        } else if (command == "listen") {
            std::string port_s = arg_value(args, "--port");
            if (port_s.empty()) { std::cerr << "usage: listen --port <port>\n"; return 1; }
            auto stats = node.listen_and_sync(std::stoi(port_s));
            print_stats(stats);
        } else if (command == "sync") {
            std::string host = arg_value(args, "--host");
            std::string port_s = arg_value(args, "--port");
            if (host.empty() || port_s.empty()) { std::cerr << "usage: sync --host <host> --port <port>\n"; return 1; }
            auto stats = node.sync_with(host, std::stoi(port_s));
            print_stats(stats);
        } else {
            print_usage();
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
