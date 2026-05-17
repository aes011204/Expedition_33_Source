#pragma once
#include "Base.h"
#include<queue>
NS_BEGIN(Engine)

struct ANode
{
    int id; //현재 위치

    float g; // 지금까지 온 비용
    float h; // 앞으로 갈 비용
    float f; // 총 비용 (g+h)

    int parent; // 어디서 왔는지
};

class AStar :
    public Base
{
public:
    AStar();
    virtual ~AStar();
public:
    vector<Vec3> FindPath(Vec3 start, Vec3 end);
private:
    vector<Vec3> Recalculate(unordered_map<int, ANode>& allNode, int goal);

    struct Compare {
        bool operator() (const ANode& a, const ANode& b) {
            return a.f > b.f;
        }
    };
    

public:

};

NS_END
