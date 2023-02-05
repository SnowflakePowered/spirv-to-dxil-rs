pub enum SpirvToDxilError {
    CompilerError(String),
    RegisterSpaceOverflow(usize)
}