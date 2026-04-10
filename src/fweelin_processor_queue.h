#ifndef __FWEELIN_PROCESSOR_QUEUE_H
#define __FWEELIN_PROCESSOR_QUEUE_H

#include <pthread.h>

class Processor;
class ProcessorItem;

struct ProcessorCommand {
  enum Type {
    CMD_ADD,
    CMD_REQUEST_DELETE
  };

  ProcessorCommand() : type(CMD_ADD), item(0), processor(0) {
  }

  Type type;
  ProcessorItem *item;
  Processor *processor;
};

class ProcessorCommandQueue {
 public:
  enum { MAX_COMMANDS = 256 };

  ProcessorCommandQueue();
  ~ProcessorCommandQueue();

  int EnqueueAdd(ProcessorItem *item);
  int EnqueueDelete(Processor *processor);
  int ReadNext(ProcessorCommand *cmd);
  int PendingCount();

 private:
  int Enqueue(const ProcessorCommand &cmd);

  pthread_mutex_t lock;
  ProcessorCommand commands[MAX_COMMANDS];
  int head;
  int tail;
  int count;
};

#endif
