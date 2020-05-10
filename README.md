# my_cjson

### 结构

* 两种实例类型

  1. cjson
     1. json项的对象
  2. cjson_hook
     1. 存malloc和free的内存分配对象

* 操作需要

  * 提供初始化malloc，realloc和free
  * 提供json模块，返回json对象，完成后调用delete
  * 提供json实例转换为传输/存储文本完成释放char*
  * 提供json实例转换为传输/存储文本，不进行格式化完成释放char*
  * 提供json实例前置缓冲和文本是否格式化，利用缓冲减少重新分配
  * 删除一个json实例和所以子集
  * 给出json实例数组或对象中的项数
  * 从项数组中利用编号索引项
    * 若没有返回null
  * 部分大小写利用项名获取项

* 创建类型

  