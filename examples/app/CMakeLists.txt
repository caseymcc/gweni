# HelloWindow (hello.cpp): the docs' hello window on gweni_app (docs/building.md
# -> "A window with gweni_app"), linking gweni_app alone as an app does. Built
# in every tree with gweni_app as a compile check: nothing runs it (it opens a
# window, and has no resources/ beside it in the tree; an app using the
# installed package gets them from gweni_add_resources).
add_executable(HelloWindow hello.cpp)
gweni_set_warnings(HelloWindow)
target_link_libraries(HelloWindow gweni_app)
