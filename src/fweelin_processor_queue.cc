#include "fweelin_processor_queue.h"

ProcessorCommandQueue::ProcessorCommandQueue()
  : head(0), tail(0), count(0) {
  pthread_mutex_init(&lock, 0);
}

ProcessorCommandQueue::~ProcessorCommandQueue() {
  pthread_mutex_destroy(&lock);
}

int ProcessorCommandQueue::Enqueue(const ProcessorCommand &cmd) {
  int ok = 0;

  pthread_mutex_lock(&lock);
  if (count < MAX_COMMANDS) {
    commands[tail] = cmd;
    tail = (tail + 1) % MAX_COMMANDS;
    count++;
    ok = 1;
  }
  pthread_mutex_unlock(&lock);

  return ok;
}

int ProcessorCommandQueue::EnqueueAdd(ProcessorItem *item) {
  ProcessorCommand cmd;
  cmd.type = ProcessorCommand::CMD_ADD;
  cmd.item = item;
  return Enqueue(cmd);
}

int ProcessorCommandQueue::EnqueueDelete(Processor *processor) {
  ProcessorCommand cmd;
  cmd.type = ProcessorCommand::CMD_REQUEST_DELETE;
  cmd.processor = processor;
  return Enqueue(cmd);
}

int ProcessorCommandQueue::ReadNext(ProcessorCommand *cmd) {
  if (cmd == 0)
    return 0;

  if (pthread_mutex_trylock(&lock) != 0)
    return 0;

  if (count == 0) {
    pthread_mutex_unlock(&lock);
    return 0;
  }

  *cmd = commands[head];
  head = (head + 1) % MAX_COMMANDS;
  count--;
  pthread_mutex_unlock(&lock);
  return 1;
}

int ProcessorCommandQueue::PendingCount() {
  int pending = 0;

  pthread_mutex_lock(&lock);
  pending = count;
  pthread_mutex_unlock(&lock);

  return pending;
}
