#ifndef __FWEELIN_STARTUP_GUARD_H
#define __FWEELIN_STARTUP_GUARD_H

typedef void (*FweelinRollbackFn)(void *ctx, int tag);

class FweelinStartupGuard {
 public:
  FweelinStartupGuard();

  void Push(FweelinRollbackFn fn, void *ctx, int tag);
  void Release();
  void Rollback();

 private:
  struct Entry {
    FweelinRollbackFn fn;
    void *ctx;
    int tag;
  };

  enum { MAX_ENTRIES = 128 };

  Entry entries[MAX_ENTRIES];
  int count;
  int released;
};

#endif
