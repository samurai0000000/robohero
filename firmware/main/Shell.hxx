/*
 * Shell.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_SHELL_HXX
#define ROBOHERO_SHELL_HXX

class Shell
{
  public:
    static Shell &instance();

    void start();

  private:
    Shell();
    Shell(const Shell &);
    Shell &operator=(const Shell &);
    static Shell _self;

    static void task(void *arg);
};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
