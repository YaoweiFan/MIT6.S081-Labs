<style> b {color: #2ca3fd}</style>
<style> o {color: orange}</style>
<style> g {color: green}</style>
<style> y {color: yellow}</style>

- <b>Which registers contain arguments to functions? For example, which register holds 13 in main's call to ```printf```?</b>
    - a2
- <b>Where is the call to function ```f``` in the assembly code for main? Where is the call to ```g```? (Hint: the compiler may inline functions.)</b>
    - 应该是被 inline 掉了
- <b>At what address is the function ```printf``` located?</b>
    - 0x628
- <b>What value is in the register ```ra``` just after the ```jalr``` to ```printf``` in ```main```?</b>
    - 0x38
- <b>Run the following code.</b>
    ```c
    unsigned int i = 0x00646c72;
    printf("H%x Wo%s", 57616, &i);
    ```
    <b>What is the output?  
    The output depends on that fact that the RISC-V is little-endian. If the RISC-V were instead big-endian what would you set ```i``` to in order to yield the same output? Would you need to change ```57616``` to a different value?</b>
    - HE110 World
    - 0x726c6400
    - need not
- <b>In the following code, what is going to be printed after ```y=```? (note: the answer is not a specific value.) Why does this happen?</b>
    ```c
    printf("x=%d y=%d", 3);
    ```
    - 5213