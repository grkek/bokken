#include "Behaviour.hpp"

namespace Bokken
{
    namespace GameObject
    {
        void Behaviour::onAttach()
        {
            transform = gameObject->getComponent<Transform>();
        }
    }
}