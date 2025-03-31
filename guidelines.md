# Coding guidelines

## Initialisation
Always initialise all variables, even if it's going to be overriden just later. Prefer completely wrong values at start.
For example, Vulkan enums have a *_MAX_ENUM. Then, init values to it, and when using assertions, use this value.
The same logic applies to class members. All of them must be initialised even though you change the value in the ctor.

### Assigning values
Avoid at all times anything that is a copy ctor. For example instead of "Obj obj = {...};", directly use "Obj obj {...};"
You can skip this for primary types (int, float, char...) as it will most likely never have any effect.

## Function naming
Use camel case for any member function, and snake case for any free function.

## Variable naming
Up to you, use something short enough and explicit. Use camel case at all times.

## Constness
Aything that can be const shall be const.

## Debugging
For debugging purpposes, add asserts every time you see an input value that could be wrong.
This means that a pointer that you accept should have an assert to check if it's not a nullptr.
To avoid such things, use a reference instead as it means that the object must stay alive during the
whole runtime of the func.
There's also the Backtrace class used to report... backtraces. You can use it during dev phase to track
possible errors.
