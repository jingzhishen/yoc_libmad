## 简介

该组件基于平头哥YoC平台使用mad以支持aac编解码功能。
MAD是一个开源的高精度 MPEG 音频解码库，支持 MPEG-1（Layer I, Layer II 和 LayerIII（也就是 MP3))。
MAD具有如下特点：
- 24位PCM输出
- 100%定点（整数）计算
- 基于ISO/IEC标准的全新实现
- 根据GNU通用公共许可证（GPL）条款提供

项目链接：https://code.aliyun.com/jingzhishen/yoc_mad

## 如何在YoC平台下编译使用

- 将mad编解码库拷贝到YoC components文件夹下
- 将ad_mad.c拷贝到components/av/avcodec文件夹下
- 修改av组件中的package.yaml文件，将该文件加入到源文件编译列表中。同时在depends项中加入mad依赖。
- 修改components/av/avcodec/avcodec_all.c，添加如下代码：

```c
/**
 * @brief  regist ad for mad
 * @return 0/-1
 */
int ad_register_mad()
{
    extern struct ad_ops ad_ops_mad;
    return ad_ops_register(&ad_ops_mad);
}
```

- 修改components/av/include/avcodec/avcodec_all.h，加入该函数的头文件声明。并修改ad_register_all内联函数，加入mad解码支持，代码如下：

```c
#if defined(CONFIG_DECODER_MAD)
    REGISTER_DECODER(MAD, mad);
#endif
```

- 修改solutions/pangu_demo/package.yaml，若使能mad解码，需加入如下配置项：

```c
CONFIG_DECODER_MAD: 1
```

