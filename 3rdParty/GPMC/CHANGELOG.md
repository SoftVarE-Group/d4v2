# Change Log

## [1.1.1] - 2022/08/26
### Added
- Add a function to contruct a d-DNNF with a variable mapping
- Deal with natural number weights of literals (-natw option)

### Changed
- Improve the preprocessor (refactoring)
- change the public methods of Counter class

## [1.1] - 2022/06/21
### Added
- Add a preprocessor for simplying input CNF. 
- Use FlowCutter for tree decomposition. The result is used for variable selection heuristics. 
- Deal with Weighted model counting

### Removed
- Options "-pp" and "-upto"