#pragma once

#include "../../canvas/Node.hpp"
#include "../SimpleStyleSheet.hpp"
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace Bokken {
namespace Renderer { class SpriteBatcher; }

namespace Canvas { namespace Components {

    /**
     * Background-rect component for the Canvas.
     */
    class View
    {
    public:
        View() = default;
        void addChild(std::shared_ptr<Node> child);
        void setStyle(const SimpleStyleSheet &style);
        static void computeNode(std::shared_ptr<Node> node, AssetPack *assets);
        static void layoutNode(std::shared_ptr<Node> node);
        std::shared_ptr<Node> toNode();

        /**
         * Emit batcher quads for this node's background and border.
         * `layer` is forwarded to the batcher for stacking — Canvas tree
         * traversal increments layer as it descends so children land
         * above parents in the same atlas batch.
         */
        static void draw(Renderer::SpriteBatcher &batcher,
                          std::shared_ptr<Node> node,
                          int layer);

    private:
        std::vector<std::shared_ptr<Node>> m_children;
        SimpleStyleSheet m_style;
    };

}}}
