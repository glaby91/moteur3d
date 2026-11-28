#include "reference_views.h"
#include "src/m3d_scene_v16.h"
#include <QCoreApplication>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc!=3)return 2;
    try{
        QString root=QString::fromLocal8Bit(argv[1]),out=QString::fromLocal8Bit(argv[2]);
        cse::androidqa::check(QDir::setCurrent(root),"asset root");
        qputenv("CSE_WARP_VK_MSAA","1");
        qputenv("CSE_WARP_VK_ALLOW_SOFTWARE_TEST","1");
        auto loaded=m3d::v16::loadScene(QDir(root).filePath("benchmark-civic/scene.json"));
        cse::androidqa::check(bool(loaded),"load scene");
        auto built=m3d::v16::buildRuntime(loaded.scene,1024);
        cse::androidqa::check(bool(built),"build scene");
        m3d::updateLighting(14.f);
        m3d::Stats st{};auto sm=m3d::buildShadow(built.runtime.renderScene,st);
        cse::androidqa::renderViews(built.runtime,sm,out);
        m3d::releaseVulkanPreviewForCurrentThread();return 0;
    }catch(const std::exception& e){qCritical()<<e.what();return 1;}
}
