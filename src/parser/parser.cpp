#include "parser.h"
#include <stdexcept>

namespace {

Command parseCommand(const std::vector<std::string>& tokens) {
    Command command;

    if (tokens.empty()) {
        throw std::runtime_error("expected a command");
    }

    if (tokens[0] == "<" || tokens[0] == ">" || tokens[0] == ">>") {
        throw std::runtime_error("expected a command before redirection");
    }

    command.program = tokens[0];

    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == ">" || tokens[i] == ">>") {
            if (i + 1 >= tokens.size() ||
                tokens[i + 1] == "<" ||
                tokens[i + 1] == ">" ||
                tokens[i + 1] == ">>" ||
                tokens[i + 1] == "2>" ||
                tokens[i + 1] == "2>>" ||
                tokens[i + 1] == "2>&1") {
                throw std::runtime_error("expected a filename after " + tokens[i]);
            }
            command.outputAppend = (tokens[i] == ">>");
            command.outputFile = tokens[++i];
        } else if (tokens[i] == "<") {
            if (i + 1 >= tokens.size() ||
                tokens[i + 1] == "<" ||
                tokens[i + 1] == ">" ||
                tokens[i + 1] == ">>" ||
                tokens[i + 1] == "2>" ||
                tokens[i + 1] == "2>>" ||
                tokens[i + 1] == "2>&1") {
                throw std::runtime_error("expected a filename after <");
            }
            command.inputFile = tokens[++i];
        } else if (tokens[i] == "2>" || tokens[i] == "2>>") {
            if (i + 1 >= tokens.size() ||
                tokens[i + 1] == "<" ||
                tokens[i + 1] == ">" ||
                tokens[i + 1] == ">>" ||
                tokens[i + 1] == "2>" ||
                tokens[i + 1] == "2>>" ||
                tokens[i + 1] == "2>&1") {
                throw std::runtime_error("expected a filename after " + tokens[i]);
            }
            command.errorAppend = (tokens[i] == "2>>");
            command.errorFile = tokens[++i];
        } else if (tokens[i] == "2>&1") {
            command.errorToStdout = true;
        } else {
            command.arguments.push_back(tokens[i]);
        }
    }

    return command;
}

} // namespace

Pipeline parse(const std::vector<std::string> &tokens)
{
    Pipeline pipeline;

    if (tokens.empty())
    {
        return pipeline;
    }

    std::vector<std::string> commandTokens;

    // A background operator is valid only at the end.
    if (tokens.back() == "&")
    {
        pipeline.background = true;
    }

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const auto &token = tokens[i];

        if (token == "&")
        {
            if (i != tokens.size() - 1)
            {
                throw std::runtime_error(
                    "'&' must appear at the end of the command");
            }

            continue;
        }

        if (token == "|")
        {
            if (commandTokens.empty())
            {
                throw std::runtime_error(
                    "expected a command before |");
            }

            pipeline.commands.push_back(
                parseCommand(commandTokens));

            commandTokens.clear();
        }
        else
        {
            commandTokens.push_back(token);
        }
    }

    if (commandTokens.empty())
    {
        throw std::runtime_error(
            "expected a command after |");
    }

    pipeline.commands.push_back(parseCommand(commandTokens));

    return pipeline;
}

CommandSequence parseSequence(const std::vector<std::string>& tokens) {
    CommandSequence sequence;
    std::vector<std::string> currentPipelineTokens;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];
        if (tok == ";" || tok == "&&" || tok == "||") {
            if (currentPipelineTokens.empty()) {
                // Handle empty commands between operators (e.g., "ls ;")
                // We still need to push a dummy or handle it. For now, we'll treat as a no-op.
                sequence.pipelines.push_back(parse(currentPipelineTokens));
            } else {
                sequence.pipelines.push_back(parse(currentPipelineTokens));
            }
            
            if (tok == ";") sequence.operators.push_back(SequenceOp::Semicolon);
            else if (tok == "&&") sequence.operators.push_back(SequenceOp::And);
            else if (tok == "||") sequence.operators.push_back(SequenceOp::Or);
            
            currentPipelineTokens.clear();
        } else {
            currentPipelineTokens.push_back(tok);
        }
    }
    
    if (!currentPipelineTokens.empty()) {
        sequence.pipelines.push_back(parse(currentPipelineTokens));
    } else if (tokens.empty() || tokens.back() == ";" || tokens.back() == "&&" || tokens.back() == "||") {
        // Trailing operator: we can add an empty pipeline to maintain operator count
        sequence.pipelines.push_back(parse(currentPipelineTokens));
    }

    return sequence;
}
