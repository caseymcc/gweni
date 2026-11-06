# Gweni examples

set(GWENI_DEMO_SOURCES
    demo/button.cpp
    demo/button.h
    demo/checkBox.cpp
    demo/checkBox.h
    demo/demoControlRenderers.cpp
    demo/collapsibleList.cpp
    demo/collapsibleList.h
    demo/colorPicker.cpp
    demo/colorPicker.h
    demo/comboBox.cpp
    demo/comboBox.h
    demo/crossSplitter.cpp
    demo/crossSplitter.h
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
    demo/slider.cpp
    demo/slider.h
    demo/statusBar.cpp
    demo/statusBar.h
    demo/tabControl.cpp
    demo/tabControl.h
    demo/textBox.cpp
    demo/textBox.h
    demo/treeControl.cpp
    demo/treeControl.h
    demo/unicodeText.cpp
    demo/unicodeText.h
    demo/window.cpp
    demo/window.h
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
