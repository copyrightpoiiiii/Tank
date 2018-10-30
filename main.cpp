#include <stack>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include "jsoncpp/json.h"

using std::cin;
using std::cout;
using std::endl;
using std::flush;
using std::getline;
using std::string;

namespace TankGame
{
using std::istream;
using std::set;
using std::stack;

#ifdef _MSC_VER
#pragma region 常量定义和说明
#endif

enum GameResult
{
	NotFinished = -2,
	Draw = -1,
	Blue = 0,
	Red = 1
};

enum FieldItem
{
	None = 0,
	Brick = 1,
	Steel = 2,
	Base = 4,
	Blue0 = 8,
	Blue1 = 16,
	Red0 = 32,
	Red1 = 64
};

template <typename T>
inline T operator~(T a) { return (T) ~(int)a; }
template <typename T>
inline T operator|(T a, T b) { return (T)((int)a | (int)b); }
template <typename T>
inline T operator&(T a, T b) { return (T)((int)a & (int)b); }
template <typename T>
inline T operator^(T a, T b) { return (T)((int)a ^ (int)b); }
template <typename T>
inline T &operator|=(T &a, T b) { return (T &)((int &)a |= (int)b); }
template <typename T>
inline T &operator&=(T &a, T b) { return (T &)((int &)a &= (int)b); }
template <typename T>
inline T &operator^=(T &a, T b) { return (T &)((int &)a ^= (int)b); }

enum Action
{
	Invalid = -2,
	Stay = -1,
	Up,
	Right,
	Down,
	Left,
	UpShoot,
	RightShoot,
	DownShoot,
	LeftShoot
};

// 坐标左上角为原点（0, 0），x 轴向右延伸，y 轴向下延伸
// Side（对战双方） - 0 为蓝，1 为红
// Tank（每方的坦克） - 0 为 0 号坦克，1 为 1 号坦克
// Turn（回合编号） - 从 1 开始

const int fieldHeight = 9, fieldWidth = 9, sideCount = 2, tankPerSide = 2;

// 基地的横坐标
const int baseX[sideCount] = {fieldWidth / 2, fieldWidth / 2};

// 基地的纵坐标
const int baseY[sideCount] = {0, fieldHeight - 1};

const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
const FieldItem tankItemTypes[sideCount][tankPerSide] = {
	{Blue0, Blue1}, {Red0, Red1}};

#ifdef _MSC_VER
#pragma endregion

#pragma region 工具函数和类
#endif

inline bool ActionIsMove(Action x)
{
	return x >= Up && x <= Left;
}

inline bool ActionIsShoot(Action x)
{
	return x >= UpShoot && x <= LeftShoot;
}

inline bool ActionDirectionIsOpposite(Action a, Action b)
{
	return a >= Up && b >= Up && (a + 2) % 4 == b % 4;
}

inline bool CoordValid(int x, int y)
{
	return x >= 0 && x < fieldWidth && y >= 0 && y < fieldHeight;
}

// 判断 item 是不是叠在一起的多个坦克
inline bool HasMultipleTank(FieldItem item)
{
	// 如果格子上只有一个物件，那么 item 的值是 2 的幂或 0
	// 对于数字 x，x & (x - 1) == 0 当且仅当 x 是 2 的幂或 0
	return !!(item & (item - 1));
}

inline int GetTankSide(FieldItem item)
{
	return item == Blue0 || item == Blue1 ? Blue : Red;
}

inline int GetTankID(FieldItem item)
{
	return item == Blue0 || item == Red0 ? 0 : 1;
}

// 获得动作的方向
inline int ExtractDirectionFromAction(Action x)
{
	if (x >= Up)
		return x % 4;
	return -1;
}

// 物件消失的记录，用于回退
struct DisappearLog
{
	FieldItem item;

	// 导致其消失的回合的编号
	int turn;

	int x, y;
	bool operator<(const DisappearLog &b) const
	{
		if (x == b.x)
		{
			if (y == b.y)
				return item < b.item;
			return y < b.y;
		}
		return x < b.x;
	}
};

#ifdef _MSC_VER
#pragma endregion

#pragma region TankField 主要逻辑类
#endif

class TankField
{
  public:
	//!//!//!// 以下变量设计为只读，不推荐进行修改 //!//!//!//

	// 游戏场地上的物件（一个格子上可能有多个坦克）
	FieldItem gameField[fieldHeight][fieldWidth] = {};

	// 坦克是否存活
	bool tankAlive[sideCount][tankPerSide] = {{true, true}, {true, true}};

	// 基地是否存活
	bool baseAlive[sideCount] = {true, true};

	// 坦克横坐标，-1表示坦克已炸
	int tankX[sideCount][tankPerSide] = {
		{fieldWidth / 2 - 2, fieldWidth / 2 + 2}, {fieldWidth / 2 + 2, fieldWidth / 2 - 2}};

	// 坦克纵坐标，-1表示坦克已炸
	int tankY[sideCount][tankPerSide] = {{0, 0}, {fieldHeight - 1, fieldHeight - 1}};

	// 当前回合编号
	int currentTurn = 1;

	// 我是哪一方
	int mySide;

	// 用于回退的log
	stack<DisappearLog> logs;

	// 过往动作（previousActions[x] 表示所有人在第 x 回合的动作，第 0 回合的动作没有意义）
	Action previousActions[101][sideCount][tankPerSide] = {{{Stay, Stay}, {Stay, Stay}}};

	//!//!//!// 以上变量设计为只读，不推荐进行修改 //!//!//!//

	// 本回合双方即将执行的动作，需要手动填入
	Action nextAction[sideCount][tankPerSide] = {{Invalid, Invalid}, {Invalid, Invalid}};

	// 判断行为是否合法（出界或移动到非空格子算作非法）
	// 未考虑坦克是否存活
	bool ActionIsValid(int side, int tank, Action act)
	{
		if (act == Invalid)
			return false;
		if (act > Left && previousActions[currentTurn - 1][side][tank] > Left) // 连续两回合射击
			return false;
		if (act == Stay || act > Left)
			return true;
		int x = tankX[side][tank] + dx[act],
			y = tankY[side][tank] + dy[act];
		return CoordValid(x, y) && gameField[y][x] == None;
	}

	// 判断 nextAction 中的所有行为是否都合法
	// 忽略掉未存活的坦克
	bool ActionIsValid()
	{
		for (int side = 0; side < sideCount; side++)
			for (int tank = 0; tank < tankPerSide; tank++)
				if (tankAlive[side][tank] && !ActionIsValid(side, tank, nextAction[side][tank]))
					return false;
		return true;
	}

  private:
	void _destroyTank(int side, int tank)
	{
		tankAlive[side][tank] = false;
		tankX[side][tank] = tankY[side][tank] = -1;
	}

	void _revertTank(int side, int tank, DisappearLog &log)
	{
		int &currX = tankX[side][tank], &currY = tankY[side][tank];
		if (tankAlive[side][tank])
			gameField[currY][currX] &= ~tankItemTypes[side][tank];
		else
			tankAlive[side][tank] = true;
		currX = log.x;
		currY = log.y;
		gameField[currY][currX] |= tankItemTypes[side][tank];
	}

  public:
	// 执行 nextAction 中指定的行为并进入下一回合，返回行为是否合法
	bool DoAction()
	{
		if (!ActionIsValid())
			return false;

		// 1 移动
		for (int side = 0; side < sideCount; side++)
			for (int tank = 0; tank < tankPerSide; tank++)
			{
				Action act = nextAction[side][tank];

				// 保存动作
				previousActions[currentTurn][side][tank] = act;
				if (tankAlive[side][tank] && ActionIsMove(act))
				{
					int &x = tankX[side][tank], &y = tankY[side][tank];
					FieldItem &items = gameField[y][x];

					// 记录 Log
					DisappearLog log;
					log.x = x;
					log.y = y;
					log.item = tankItemTypes[side][tank];
					log.turn = currentTurn;
					logs.push(log);

					// 变更坐标
					x += dx[act];
					y += dy[act];

					// 更换标记（注意格子可能有多个坦克）
					gameField[y][x] |= log.item;
					items &= ~log.item;
				}
			}

		// 2 射♂击
		set<DisappearLog> itemsToBeDestroyed;
		for (int side = 0; side < sideCount; side++)
			for (int tank = 0; tank < tankPerSide; tank++)
			{
				Action act = nextAction[side][tank];
				if (tankAlive[side][tank] && ActionIsShoot(act))
				{
					int dir = ExtractDirectionFromAction(act);
					int x = tankX[side][tank], y = tankY[side][tank];
					bool hasMultipleTankWithMe = HasMultipleTank(gameField[y][x]);
					while (true)
					{
						x += dx[dir];
						y += dy[dir];
						if (!CoordValid(x, y))
							break;
						FieldItem items = gameField[y][x];
						if (items != None)
						{
							// 对射判断
							if (items >= Blue0 &&
								!hasMultipleTankWithMe && !HasMultipleTank(items))
							{
								// 自己这里和射到的目标格子都只有一个坦克
								Action theirAction = nextAction[GetTankSide(items)][GetTankID(items)];
								if (ActionIsShoot(theirAction) &&
									ActionDirectionIsOpposite(act, theirAction))
								{
									// 而且我方和对方的射击方向是反的
									// 那么就忽视这次射击
									break;
								}
							}

							// 标记这些物件要被摧毁了（防止重复摧毁）
							for (int mask = 1; mask <= Red1; mask <<= 1)
								if (items & mask)
								{
									DisappearLog log;
									log.x = x;
									log.y = y;
									log.item = (FieldItem)mask;
									log.turn = currentTurn;
									itemsToBeDestroyed.insert(log);
								}
							break;
						}
					}
				}
			}

		for (auto &log : itemsToBeDestroyed)
		{
			switch (log.item)
			{
			case Base:
			{
				int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
				baseAlive[side] = false;
				break;
			}
			case Blue0:
				_destroyTank(Blue, 0);
				break;
			case Blue1:
				_destroyTank(Blue, 1);
				break;
			case Red0:
				_destroyTank(Red, 0);
				break;
			case Red1:
				_destroyTank(Red, 1);
				break;
			case Steel:
				continue;
			default:;
			}
			gameField[log.y][log.x] &= ~log.item;
			logs.push(log);
		}

		for (int side = 0; side < sideCount; side++)
			for (int tank = 0; tank < tankPerSide; tank++)
				nextAction[side][tank] = Invalid;

		currentTurn++;
		return true;
	}

	// 回到上一回合
	bool Revert()
	{
		if (currentTurn == 1)
			return false;

		currentTurn--;
		while (!logs.empty())
		{
			DisappearLog &log = logs.top();
			if (log.turn == currentTurn)
			{
				logs.pop();
				switch (log.item)
				{
				case Base:
				{
					int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
					baseAlive[side] = true;
					gameField[log.y][log.x] = Base;
					break;
				}
				case Brick:
					gameField[log.y][log.x] = Brick;
					break;
				case Blue0:
					_revertTank(Blue, 0, log);
					break;
				case Blue1:
					_revertTank(Blue, 1, log);
					break;
				case Red0:
					_revertTank(Red, 0, log);
					break;
				case Red1:
					_revertTank(Red, 1, log);
					break;
				default:;
				}
			}
			else
				break;
		}
		return true;
	}

	// 游戏是否结束？谁赢了？
	GameResult GetGameResult()
	{
		bool fail[sideCount] = {};
		for (int side = 0; side < sideCount; side++)
			if ((!tankAlive[side][0] && !tankAlive[side][1]) || !baseAlive[side])
				fail[side] = true;
		if (fail[0] == fail[1])
			return fail[0] || currentTurn > 100 ? Draw : NotFinished;
		if (fail[Blue])
			return Red;
		return Blue;
	}

	// 三个 int 表示场地 01 矩阵（每个 int 用 27 位表示 3 行）
	TankField(int hasBrick[3], int mySide) : mySide(mySide)
	{
		for (int i = 0; i < 3; i++)
		{
			int mask = 1;
			for (int y = i * 3; y < (i + 1) * 3; y++)
			{
				for (int x = 0; x < fieldWidth; x++)
				{
					if (hasBrick[i] & mask)
						gameField[y][x] = Brick;
					mask <<= 1;
				}
			}
		}
		for (int side = 0; side < sideCount; side++)
		{
			for (int tank = 0; tank < tankPerSide; tank++)
				gameField[tankY[side][tank]][tankX[side][tank]] = tankItemTypes[side][tank];
			gameField[baseY[side]][baseX[side]] = Base;
		}
		gameField[baseY[0] + 1][baseX[0]] = gameField[baseY[1] - 1][baseX[1]] = Steel;
	}

	// 打印场地
	void DebugPrint()
	{
#ifndef _BOTZONE_ONLINE
		const string side2String[] = {"蓝", "红"};
		const string boolean2String[] = {"已炸", "存活"};
		const char *boldHR = "==============================";
		const char *slimHR = "------------------------------";
		cout << boldHR << endl
			 << "图例：" << endl
			 << ". - 空\t# - 砖\t% - 钢\t* - 基地\t@ - 多个坦克" << endl
			 << "b - 蓝0\tB - 蓝1\tr - 红0\tR - 红1" << endl
			 << slimHR << endl;
		for (int y = 0; y < fieldHeight; y++)
		{
			for (int x = 0; x < fieldWidth; x++)
			{
				switch (gameField[y][x])
				{
				case None:
					cout << '.';
					break;
				case Brick:
					cout << '#';
					break;
				case Steel:
					cout << '%';
					break;
				case Base:
					cout << '*';
					break;
				case Blue0:
					cout << 'b';
					break;
				case Blue1:
					cout << 'B';
					break;
				case Red0:
					cout << 'r';
					break;
				case Red1:
					cout << 'R';
					break;
				default:
					cout << '@';
					break;
				}
			}
			cout << endl;
		}
		cout << slimHR << endl;
		for (int side = 0; side < sideCount; side++)
		{
			cout << side2String[side] << "：基地" << boolean2String[baseAlive[side]];
			for (int tank = 0; tank < tankPerSide; tank++)
				cout << ", 坦克" << tank << boolean2String[tankAlive[side][tank]];
			cout << endl;
		}
		cout << "当前回合：" << currentTurn << "，";
		GameResult result = GetGameResult();
		if (result == -2)
			cout << "游戏尚未结束" << endl;
		else if (result == -1)
			cout << "游戏平局" << endl;
		else
			cout << side2String[result] << "方胜利" << endl;
		cout << boldHR << endl;
#endif
	}
};

#ifdef _MSC_VER
#pragma endregion
#endif

TankField *field;

#ifdef _MSC_VER
#pragma region 与平台交互部分
#endif

// 内部函数
namespace Internals
{
Json::Reader reader;
#ifdef _BOTZONE_ONLINE
Json::FastWriter writer;
#else
Json::StyledWriter writer;
#endif

void _processRequestOrResponse(Json::Value &value, bool isOpponent)
{
	if (value.isArray())
	{
		if (!isOpponent)
		{
			for (int tank = 0; tank < tankPerSide; tank++)
				field->nextAction[field->mySide][tank] = (Action)value[tank].asInt();
		}
		else
		{
			for (int tank = 0; tank < tankPerSide; tank++)
				field->nextAction[1 - field->mySide][tank] = (Action)value[tank].asInt();
			field->DoAction();
		}
	}
	else
	{
		// 是第一回合，裁判在介绍场地
		int hasBrick[3];
		for (int i = 0; i < 3; i++)
			hasBrick[i] = value["field"][i].asInt();
		field = new TankField(hasBrick, value["mySide"].asInt());
	}
}

// 请使用 SubmitAndExit 或者 SubmitAndDontExit
void _submitAction(Action tank0, Action tank1, string debug = "", string data = "", string globalData = "")
{
	Json::Value output(Json::objectValue), response(Json::arrayValue);
	response[0U] = tank0;
	response[1U] = tank1;
	output["response"] = response;
	if (!debug.empty())
		output["debug"] = debug;
	if (!data.empty())
		output["data"] = data;
	if (!globalData.empty())
		output["globalData"] = globalData;
	cout << writer.write(output) << endl;
}
} // namespace Internals

// 从输入流（例如 cin 或者 fstream）读取回合信息，存入 TankField，并提取上回合存储的 data 和 globaldata
// 本地调试的时候支持多行，但是最后一行需要以没有缩进的一个"}"或"]"结尾
void ReadInput(istream &in, string &outData, string &outGlobalData)
{
	Json::Value input;
	string inputString;
	do
	{
		getline(in, inputString);
	} while (inputString.empty());
#ifndef _BOTZONE_ONLINE
	// 猜测是单行还是多行
	char lastChar = inputString[inputString.size() - 1];
	if (lastChar != '}' && lastChar != ']')
	{
		// 第一行不以}或]结尾，猜测是多行
		string newString;
		do
		{
			getline(in, newString);
			inputString += newString;
		} while (newString != "}" && newString != "]");
	}
#endif
	Internals::reader.parse(inputString, input);

	if (input.isObject())
	{
		Json::Value requests = input["requests"], responses = input["responses"];
		if (!requests.isNull() && requests.isArray())
		{
			size_t i, n = requests.size();
			for (i = 0; i < n; i++)
			{
				Internals::_processRequestOrResponse(requests[(int)i], true);
				if (i < n - 1)
					Internals::_processRequestOrResponse(responses[(int)i], false);
			}
			outData = input["data"].asString();
			outGlobalData = input["globaldata"].asString();
			return;
		}
	}
	Internals::_processRequestOrResponse(input, true);
}

// 提交决策并退出，下回合时会重新运行程序
void SubmitAndExit(Action tank0, Action tank1, string debug = "", string data = "", string globalData = "")
{
	Internals::_submitAction(tank0, tank1, debug, data, globalData);
	exit(0);
}

// 提交决策，下回合时程序继续运行（需要在 Botzone 上提交 Bot 时选择“允许长时运行”）
// 如果游戏结束，程序会被系统杀死
void SubmitAndDontExit(Action tank0, Action tank1)
{
	Internals::_submitAction(tank0, tank1);
	field->nextAction[field->mySide][0] = tank0;
	field->nextAction[field->mySide][1] = tank1;
	cout << ">>>BOTZONE_REQUEST_KEEP_RUNNING<<<" << endl;
}
#ifdef _MSC_VER
#pragma endregion
#endif
} // namespace TankGame

int RandBetween(int from, int to)
{
	return rand() % (to - from) + from;
}

TankGame::Action RandAction(int tank)
{
	while (true)
	{
		auto act = (TankGame::Action)RandBetween(TankGame::Stay, TankGame::LeftShoot + 1);
		if (TankGame::field->ActionIsValid(TankGame::field->mySide, tank, act))
			return act;
	}
}

int dis[15][15][15][15];
int attack_distance[15][2];
int safty_block[15][15];
static int enemy_side, my_side;
std::pair<int, int> enemy_tank[2], my_tank[2];
//记录上一回合敌方坦克的位置，预判它下一回合可能的移动方向
std::pair<int, int> last_enemy_tank[2];
std::pair<int, int> predict_enemy_tank[2];

bool update_safty(TankGame::FieldItem item, int x, int y, int add)
{
	switch (item)
	{
	case TankGame::None:
		safty_block[x][y] += add;
		break;
	case TankGame::Brick:
		safty_block[x][y] += add;
		return 1;
	case TankGame::Steel:
		return 1;
	case TankGame::Base:
		safty_block[x][y] += add;
		break;
	default:
		safty_block[x][y] += add;
		return 1;
	}
	return 0;
}

void update_distance()
{ //用Floyd算出任意两点之间的距离（需要走的回合数）
	for (int i = 0; i < TankGame::fieldHeight; i++)
		for (int j = 0; j < TankGame::fieldWidth; j++)
		{
			dis[i][j][i][j] = 0;
			for (int k = 0; k < 4; k++)
			{
				int tmpy = i + TankGame::dy[k], tmpx = j + TankGame::dx[k];
				if (!TankGame::CoordValid(tmpx, tmpy))
					continue;
				if (TankGame::field->gameField[tmpy][tmpx] == TankGame::Steel)
					continue;
				if (TankGame::field->gameField[tmpy][tmpx] == TankGame::Brick)
					dis[i][j][tmpy][tmpx] = 2; //砖块需要两个回合
				else
					dis[i][j][tmpy][tmpx] = 1; //其他需要一个回合
			}
		}
	//Floyd
	for (int ki = 0; ki < TankGame::fieldHeight; ki++)
		for (int kj = 0; kj < TankGame::fieldWidth; kj++)
			for (int i = 0; i < TankGame::fieldHeight; i++)
				for (int j = 0; j < TankGame::fieldWidth; j++)
					for (int ii = 0; ii < TankGame::fieldHeight; ii++)
						for (int jj = 0; jj < TankGame::fieldWidth; jj++)
							dis[i][j][ii][jj] = std::min(dis[i][j][ii][jj], dis[i][j][ki][kj] + dis[ki][kj][ii][jj]);
}

void update_attack_distance()
{
	//计算出从第一行直接攻击基地，中间有多少个砖块需要被打掉
	int cnt[9];
	memset(cnt, 0, sizeof(cnt));
	for (int i = 0; i < 9; i++)
		if (TankGame::field->gameField[0][i] == TankGame::Brick)
			cnt[i] = cnt[i - 1] + 1;
		else
			cnt[i] = cnt[i - 1];
	for (int i = 0; i < 9; i++)
		attack_distance[i][0] = cnt[std::max(i, 4)] - cnt[std::min(i, 4)];
	//计算出从最后一行直接攻击基地，中间有多少个砖块需要被打掉
	memset(cnt, 0, sizeof(cnt));
	for (int i = 0; i < 9; i++)
		if (TankGame::field->gameField[8][i] == TankGame::Brick)
			cnt[i] = cnt[i - 1] + 1;
		else
			cnt[i] = cnt[i - 1];
	for (int i = 0; i < 9; i++)
		attack_distance[i][1] = cnt[std::max(i, 4)] - cnt[std::min(i, 4)];
}

std::pair<int, int> choose_moving_target(std::pair<int, int> tank_position, int side)
{ //找到从当前位置最快可以打击到对方基地的目标点
	std::pair<int, int> target;
	int min_distance = 0x3ff;
	for (int i = 0; i < 9; i++)
	{
		//cout<<dis[tank_position.first][tank_position.second][id?8:0][i]<<endl;
		if (dis[tank_position.first][tank_position.second][side ? 0 : 8][i] + attack_distance[i][side^1] * 2 < min_distance)
		{
			target = std::make_pair(side ? 0 : 8, i);
			min_distance = dis[tank_position.first][tank_position.second][side ? 0 : 8][i] + attack_distance[i][side^1] * 2;
		}
	}
	return target;
}

bool judge_right_path(std::pair<int, int> target, std::pair<int, int> tank_position, std::pair<int, int> tmp_point)
{
	//std::cout<<target.first<<" "<<target.second<<" "<<tank_position.first<<" "<<tank_position.second<<" "<<tmp_point.first<<" "<<tmp_point.second<<std::endl;
	int dis1 = dis[tank_position.first][tank_position.second][tmp_point.first][tmp_point.second];
	int dis2 = dis[tmp_point.first][tmp_point.second][target.first][target.second];
	int dis3 = dis[tank_position.first][tank_position.second][target.first][target.second];
	//cout<<dis1<<" "<<dis2<<" "<<dis3<<endl;
	return (dis1 + dis2) == dis3;
}

void find_enemy_move(int tank)
{
	int abs_i = enemy_tank[tank].first - last_enemy_tank[tank].first;
	int abs_j = enemy_tank[tank].second - last_enemy_tank[tank].second;
	predict_enemy_tank[tank].first = enemy_tank[tank].first + abs_i;
	predict_enemy_tank[tank].second = enemy_tank[tank].second + abs_j;
	if (TankGame::field->gameField[predict_enemy_tank[tank].first][predict_enemy_tank[tank].second] == TankGame::None)
		return;
	predict_enemy_tank[tank] = choose_moving_target(enemy_tank[tank], enemy_side);
	for (int k = 0; k < 4; k++)
	{
		int tmpy = enemy_tank[tank].first + TankGame::dy[k], tmpx = enemy_tank[tank].second + TankGame::dx[k];
		if (!TankGame::CoordValid(tmpx, tmpy))
			continue;
		if (TankGame::field->gameField[tmpy][tmpx] == TankGame::Steel)
			continue;
		if (TankGame::field->gameField[tmpy][tmpx] != TankGame::Brick)
		{
			if (judge_right_path(predict_enemy_tank[tank], enemy_tank[tank], std::make_pair(tmpy, tmpx)))
			{
				predict_enemy_tank[tank] = std::make_pair(tmpy, tmpx);
				return;
			}
		}
	}
	enemy_tank[tank] = enemy_tank[tank];
}

void update_info()
{ //更新对方坦克和自己坦克的坐标，以及distance的初始化
	enemy_side = TankGame::field->mySide ^ 1;
	my_side = TankGame::field->mySide;
	enemy_tank[0] = std::make_pair(TankGame::field->tankY[enemy_side][0], TankGame::field->tankX[enemy_side][0]);
	enemy_tank[1] = std::make_pair(TankGame::field->tankY[enemy_side][1], TankGame::field->tankX[enemy_side][1]);
	my_tank[0] = std::make_pair(TankGame::field->tankY[my_side][0], TankGame::field->tankX[my_side][0]);
	my_tank[1] = std::make_pair(TankGame::field->tankY[my_side][1], TankGame::field->tankX[my_side][1]);
	memset(dis, 0x3f, sizeof(dis));
	//预处理出对方坦克的火力范围和我方坦克的火力范围
	//1为敌人火力，2为我方火力，3为同时覆盖，似乎没用
	memset(safty_block, 0, sizeof(safty_block));
	update_distance();
	update_attack_distance();
	find_enemy_move(0);
	find_enemy_move(1);
	for (int j = 0; j < 2; j++)
	{
		for (int i = predict_enemy_tank[j].first + 1; i < 9; i++)
			if (update_safty(TankGame::field->gameField[i][predict_enemy_tank[j].second], i, predict_enemy_tank[j].second, 1))
				break;

		for (int i = predict_enemy_tank[j].first - 1; i >= 0; i--)
			if (update_safty(TankGame::field->gameField[i][predict_enemy_tank[j].second], i, predict_enemy_tank[j].second, 1))
				break;

		for (int i = predict_enemy_tank[j].second + 1; i < 9; i++)
			if (update_safty(TankGame::field->gameField[predict_enemy_tank[j].first][i], predict_enemy_tank[j].first, i, 1))
				break;

		for (int i = predict_enemy_tank[j].second - 1; i >= 0; i--)
			if (update_safty(TankGame::field->gameField[predict_enemy_tank[j].first][i], predict_enemy_tank[j].first, i, 1))
				break;
	}
	for (int j = 0; j < 2; j++)
	{
		for (int i = my_tank[j].first + 1; i < 9; i++)
			if (update_safty(TankGame::field->gameField[i][my_tank[j].second], i, my_tank[j].second, 2))
				break;

		for (int i = my_tank[j].first - 1; i >= 0; i--)
			if (update_safty(TankGame::field->gameField[i][my_tank[j].second], i, my_tank[j].second, 2))
				break;

		for (int i = my_tank[j].second + 1; i < 9; i++)
			if (update_safty(TankGame::field->gameField[my_tank[j].first][i], my_tank[j].first, i, 2))
				break;

		for (int i = my_tank[j].second - 1; i >= 0; i--)
			if (update_safty(TankGame::field->gameField[my_tank[j].first][i], my_tank[j].first, i, 2))
				break;
	}
}

TankGame::Action choose_move_direction(int x)
{
	if (x == 0)
		return TankGame::Up;
	else if (x == 1)
		return TankGame::Right;
	else if (x == 2)
		return TankGame::Down;
	else
		return TankGame::Left;
}

TankGame::Action choose_shoot_direction(int side, int tank, int x)
{
	if (x == 0)
		return TankGame::field->ActionIsValid(side, tank, TankGame::UpShoot) ? TankGame::UpShoot : TankGame::Stay;
	else if (x == 1)
		return TankGame::field->ActionIsValid(side, tank, TankGame::RightShoot) ? TankGame::RightShoot : TankGame::Stay;
	else if (x == 2)
		return TankGame::field->ActionIsValid(side, tank, TankGame::DownShoot) ? TankGame::DownShoot : TankGame::Stay;
	else
		return TankGame::field->ActionIsValid(side, tank, TankGame::LeftShoot) ? TankGame::LeftShoot : TankGame::Stay;
}

TankGame::Action check_brick_between_two_tank(std::pair<int, int> my_tank, std::pair<int, int> enemy_tank)
{
	//如果在同一行
	if (my_tank.first == enemy_tank.first)
	{
		if (my_tank.second < enemy_tank.second)
			return TankGame::RightShoot;
		else
			return TankGame::LeftShoot;
	}
	//如果在同一列
	if (my_tank.second == enemy_tank.second)
	{
		if (my_tank.first < enemy_tank.first)
			return TankGame::DownShoot;
		else
			return TankGame::UpShoot;
	}
	return TankGame::Invalid;
}

TankGame::Action attack(int side, int tank)
{
	//对于敌方的第一个坦克
	if (safty_block[predict_enemy_tank[0].first][predict_enemy_tank[0].second] >= 2)
		return check_brick_between_two_tank(my_tank[tank], predict_enemy_tank[0]);
	//对于敌方的第二个坦克
	if (safty_block[predict_enemy_tank[1].first][predict_enemy_tank[1].second] >= 2)
		return check_brick_between_two_tank(my_tank[tank], predict_enemy_tank[1]);
	return TankGame::Invalid;
}

bool is_position_safe(std::pair<int, int> pos)
{
	if (safty_block[pos.first][pos.second] == 1)
		return 0;
	else if (safty_block[pos.first][pos.second] == 3)
		return 0;
	else
		return 1;
}

TankGame::Action MyAction(int side, int tank)
{
	if (TankGame::field->ActionIsValid(side, tank, TankGame::LeftShoot))
	{ //检测是否可以进行攻击
		TankGame::Action rec = attack(side, tank);
		//cout<<rec<<endl;
		if (rec != TankGame::Invalid)
			return rec;
	}
	//auto act=(TankGame::Stay);
	std::pair<int, int> tank_position = my_tank[tank];
	std::pair<int, int> target = choose_moving_target(tank_position, TankGame::field->mySide);
	//ßcout << target.first << " " << target.second << endl;
	if (target == tank_position)
	{ //说明坦克已经到达最后目标，只需要朝着对方基地射击即可
		if (tank_position.second < 4)
		{
			return TankGame::field->ActionIsValid(side, tank, TankGame::RightShoot) ? TankGame::RightShoot : TankGame::Stay;
		}
		else
		{
			return TankGame::field->ActionIsValid(side, tank, TankGame::LeftShoot) ? TankGame::LeftShoot : TankGame::Stay;
		}
	}
	else
	{
		//先找是否可以直接进行移动，而不需要发射炮弹（即有多条道路）
		for (int k = 0; k < 4; k++)
		{
			int tmpy = tank_position.first + TankGame::dy[k], tmpx = tank_position.second + TankGame::dx[k];
			if (!TankGame::CoordValid(tmpx, tmpy))
				continue;
			if (TankGame::field->gameField[tmpy][tmpx] == TankGame::Steel)
				continue;
			if (!is_position_safe(std::make_pair(tmpy, tmpx)))
				continue;
			if (TankGame::field->gameField[tmpy][tmpx] != TankGame::Brick)
			{
				if (judge_right_path(target, tank_position, std::make_pair(tmpy, tmpx)))
					return choose_move_direction(k);
			}
		}
		//否则发射炮弹开路
		for (int k = 0; k < 4; k++)
		{
			int tmpy = tank_position.first + TankGame::dy[k], tmpx = tank_position.second + TankGame::dx[k];
			if (!TankGame::CoordValid(tmpx, tmpy))
				continue;
			if (TankGame::field->gameField[tmpy][tmpx] == TankGame::Steel)
				continue;
			if (!is_position_safe(std::make_pair(tmpy, tmpx)))
				continue;
			if (TankGame::field->gameField[tmpy][tmpx] == TankGame::Brick)
			{
				if (judge_right_path(target, tank_position, std::make_pair(tmpy, tmpx)))
					return choose_shoot_direction(side, tank, k);
			}
		}
	}
	//cout<<"error"<<endl;
	return TankGame::Stay;
}

int main()
{
	srand((unsigned)time(nullptr));
	while (true)
	{
		string data, globaldata;
		TankGame::ReadInput(cin, data, globaldata);
		//Debug开关
		//TankGame::field->DebugPrint();
		update_info();
		//std::cout<<MyAction(TankGame::field->mySide, 0)<<" "<<MyAction(TankGame::field->mySide, 1)<<std::endl;
		TankGame::SubmitAndDontExit(MyAction(TankGame::field->mySide, 0), MyAction(TankGame::field->mySide, 1));
		//TankGame::SubmitAndDontExit(RandAction( 0), RandAction( 1));
	}
}