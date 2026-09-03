/*
 * Web.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_WEB_HXX
#define ROBOHERO_WEB_HXX

class Web
{
  public:
    static Web &instance();

    void start();

  private:
    Web();
    Web(const Web &);
    Web &operator=(const Web &);
    static Web _self;

    void *_server;
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
