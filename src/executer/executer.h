#ifndef EXECUTER_H
#define EXECUTER_H

#include "../parser/command.h"

int executeCommand(const Command& command);
int executePipeline(const Pipeline& pipeline);

void installChildHandler();
void reapBackgroundChildren();
void listJobs();
bool resumeJobInBackground(int jobId = -1);
bool resumeJobInForeground(int jobId = -1);
bool killJob(int jobId, int signalNumber = 15);

#endif
