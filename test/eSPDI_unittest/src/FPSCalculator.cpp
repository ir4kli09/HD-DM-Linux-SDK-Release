#include "FPSCalculator.h"

FPSCalculator::FPSCalculator():
m_fFPS(0.0f)
{

}

void FPSCalculator::clock()
{
    // update fps
    if (m_receiveTimeStorage.size() > MAX_RECEIVE_TIME_LIMIT) m_receiveTimeStorage.pop_front();

    m_receiveTimeStorage.push_back(std::clock());
    if (!m_receiveTimeStorage.empty())
        m_fFPS = ((m_receiveTimeStorage.size() - 1) * 1000.0f) /
                 (((m_receiveTimeStorage.back() - m_receiveTimeStorage.front()) / CLOCKS_PER_SEC) * 1000.0f);
}

float FPSCalculator::GetFPS()
{
    return m_fFPS;
}
