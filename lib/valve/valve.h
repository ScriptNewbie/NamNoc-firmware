class Valve
{
    bool opened = false;

public:
    Valve();
    void open();
    void close();
    bool isOpened();
    void handlEvents();
};