﻿// configuration.h
// 提供总体配置

#pragma once

// 棋盘大小
// 列
#define BOARD_SIZE_COL	9
// 行
#define BOARD_SIZE_ROW	9
// 棋子大小 半径
#define CHESS_RADIUS	15

//
#define HASH_SIZE_ORDER		20
#define HASH_SIZE			(1<< HASH_SIZE_ORDER)
#define HASH_SIZE_MASK		(HASH_SIZE - 2)