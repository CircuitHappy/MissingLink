#include <iostream>
#include <cstdlib>
#include <memory>
#include <chrono>
#include <atomic>
#include <rtmidi/RtMidi.h>

namespace MissingLink {

class MidiOut {

  public:

    MidiOut();
    virtual ~MidiOut();

    void ClockOut();
    void StartTransport();
    void StopTransport();
    void AllNotesOff();
    void CheckPorts();

  protected:

    std::unique_ptr<RtMidiOut> m_pMidiOut;
    std::vector<unsigned char> m_message;
    std::atomic<bool> m_foundMidiPort;
    unsigned int m_numPorts;

    void open();
    void close();

};

}//namespaces
