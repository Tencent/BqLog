import { hilog } from '@kit.PerformanceAnalysisKit';
import module from 'libentry.so'
@Entry
@Component
struct Index {
  @State message: string = 'Hello World';

  build() {
    Row() {
      Column() {
        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(async () => {
            module.add(3,4);
            let ver = module.api_get_log_version();
            hilog.info(0x0000, 'testTag', 'Test NAPI 2 + 3 = %{public}d', module.add(2, 3));
            hilog.info(0x0000, 'testTag', 'version='+ver);
          })
      }
      .width('100%')
    }
    .height('100%')
  }
}
