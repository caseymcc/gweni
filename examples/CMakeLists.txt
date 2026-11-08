# Gweni examples

set(GWENI_DEMO_SOURCES
    demo/button.cpp
    demo/button.h
    demo/checkBox.cpp
    demo/checkBox.h
    demo/demoControlRenderers.cpp
    demo/collapsibleList.cpp
    demo/collapsibleList.h
    demo/colorEdit.cpp
    demo/colorEdit.h
    demo/colorPicker.cpp
    demo/colorPicker.h
    demo/comboBox.cpp
    demo/comboBox.h
    demo/contextMenu.cpp
    demo/contextMenu.h
    demo/popup.cpp
    demo/popup.h
    demo/crossSplitter.cpp
    demo/crossSplitter.h
    demo/dataTable.cpp
    demo/dataTable.h
    demo/demo.cpp
    demo/demo.h
    demo/groupBox.cpp
    demo/groupBox.h
    demo/imagePanel.cpp
    demo/imagePanel.h
    demo/label.cpp
    demo/label.h
    demo/labelMultiline.cpp
    demo/labelMultiline.h
    demo/layoutDemo.cpp
    demo/layoutDemo.h
    demo/listBox.cpp
    demo/listBox.h
    demo/menuStrip.cpp
    demo/menuStrip.h
    demo/numeric.cpp
    demo/numeric.h
    demo/pageControl.cpp
    demo/pageControl.h
    demo/progressBar.cpp
    demo/progressBar.h
    demo/properties.cpp
    demo/properties.h
    demo/radioButton.cpp
    demo/radioButton.h
    demo/scrollControl.cpp
    demo/scrollControl.h
    demo/selectable.cpp
    demo/selectable.h
    demo/separator.cpp
    demo/separator.h
    demo/showcase.cpp
    demo/showcase.h
    demo/showcaseButtons.cpp
    demo/showcaseButtons.h
    demo/showcaseDisplay.cpp
    demo/showcaseDisplay.h
    demo/showcaseContainers.cpp
    demo/showcaseContainers.h
    demo/showcaseInput.cpp
    demo/showcaseInput.h
    demo/showcaseWindows.cpp
    demo/showcaseWindows.h
    demo/slider.cpp
    demo/slider.h
    demo/sliderText.cpp
    demo/sliderText.h
    demo/statusBar.cpp
    demo/statusBar.h
    demo/tabClose.cpp
    demo/tabClose.h
    demo/tabControl.cpp
    demo/tabControl.h
    demo/tabOverflow.cpp
    demo/tabOverflow.h
    demo/textBox.cpp
    demo/textBox.h
    demo/textDemo.cpp
    demo/textDemo.h
    demo/textSizes.cpp
    demo/textSizes.h
    demo/treeControl.cpp
    demo/treeControl.h
    demo/unicodeText.cpp
    demo/unicodeText.h
    demo/virtualList.cpp
    demo/virtualList.h
    demo/window.cpp
    demo/window.h
    demo/windowButtons.cpp
    demo/windowButtons.h
)

add_library(gweni_demo STATIC
    ${GWENI_DEMO_SOURCES}
)

source_group("" FILES ${GWENI_DEMO_SOURCES})

gweni_set_warnings(gweni_demo)

target_include_directories(gweni_demo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:examples>
)
target_link_libraries(gweni_demo gweni)

if(GWENI_RENDER_OPENGL_CORE)
    add_subdirectory(openglcore)
endif()
