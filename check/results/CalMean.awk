BEGIN {
    # 位移量 c，可以根据数据集选择
    c = 1

    # 初始化变量
    for (i = 1; i <= 7; i++) {
        product[i] = 1
        count[i] = 0
    }
}

{
    # 遍历每一列
    for (i = 1; i <= NF; i++) {
        # 累乘 (数值 + 位移量)
        product[i] *= ($i + c)
        count[i]++
    }
}

END {
    # 计算每列的位移几何平均
    for (i = 1; i <= NF; i++) {
        # 位移几何平均公式
        shifted_geomean = exp(log(product[i])/count[i]) - c
        printf "%.2f ", shifted_geomean
    }
    printf "\n"
}

