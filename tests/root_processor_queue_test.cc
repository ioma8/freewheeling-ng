#include "fweelin_processor_queue.h"
#include <assert.h>

int main() {
  ProcessorCommandQueue queue;
  ProcessorCommand cmd;
  ProcessorItem *item1 = (ProcessorItem *) 1;
  ProcessorItem *item2 = (ProcessorItem *) 2;
  Processor *proc = (Processor *) 3;

  assert(queue.PendingCount() == 0);
  assert(queue.EnqueueAdd(item1) == 1);
  assert(queue.EnqueueAdd(item2) == 1);
  assert(queue.EnqueueDelete(proc) == 1);
  assert(queue.PendingCount() == 3);

  assert(queue.ReadNext(&cmd) == 1);
  assert(cmd.type == ProcessorCommand::CMD_ADD);
  assert(cmd.item == item1);

  assert(queue.ReadNext(&cmd) == 1);
  assert(cmd.type == ProcessorCommand::CMD_ADD);
  assert(cmd.item == item2);

  assert(queue.ReadNext(&cmd) == 1);
  assert(cmd.type == ProcessorCommand::CMD_REQUEST_DELETE);
  assert(cmd.processor == proc);

  assert(queue.PendingCount() == 0);
  return 0;
}
