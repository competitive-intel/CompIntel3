#pragma once
namespace compintel::ui
{
inline constexpr wchar_t UiMarkup[] = LR"XAML(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="Root">
  <Grid.RowDefinitions>
    <RowDefinition Height="Auto"/>
    <RowDefinition Height="*"/>
  </Grid.RowDefinitions>
  <TitleBar x:Name="AppTitleBar" Title="CompIntel3" Subtitle="算法对弈工作台">
    <TitleBar.IconSource>
      <SymbolIconSource Symbol="AllApps"/>
    </TitleBar.IconSource>
  </TitleBar>
  <NavigationView x:Name="AppNavigation" Grid.Row="1" PaneDisplayMode="Auto"
                 OpenPaneLength="240" CompactModeThresholdWidth="720" ExpandedModeThresholdWidth="1200"
                 IsBackButtonVisible="Collapsed" IsSettingsVisible="False" AlwaysShowHeader="False">
    <NavigationView.MenuItems>
      <NavigationViewItemHeader Content="工作空间"/>
      <NavigationViewItem x:Name="NavArena" Content="对弈工作台" Tag="0">
        <NavigationViewItem.Icon>
          <FontIcon Glyph="&#xE7FC;"/>
        </NavigationViewItem.Icon>
      </NavigationViewItem>
      <NavigationViewItem x:Name="NavEditor" Content="程序实验室" Tag="1">
        <NavigationViewItem.Icon>
          <FontIcon Glyph="&#xE943;"/>
        </NavigationViewItem.Icon>
      </NavigationViewItem>
      <NavigationViewItem x:Name="NavReplay" Content="对局回放" Tag="2" Icon="Clock"/>
      <NavigationViewItem x:Name="NavRules" Content="规则与协议" Tag="3" Icon="Library"/>
    </NavigationView.MenuItems>
    <NavigationView.FooterMenuItems>
      <NavigationViewItem x:Name="NavSettings" Content="设置" Tag="4" Icon="Setting"/>
    </NavigationView.FooterMenuItems>
    <Grid x:Name="PageHost" Padding="24,16,24,24" RowSpacing="16">
      <Grid.RowDefinitions>
        <RowDefinition Height="Auto"/>
        <RowDefinition Height="Auto"/>
        <RowDefinition Height="*"/>
      </Grid.RowDefinitions>
      <StackPanel Spacing="4">
        <TextBlock x:Name="PageTitle" Text="对弈工作台" Style="{StaticResource TitleTextBlockStyle}" AutomationProperties.HeadingLevel="Level1"/>
        <TextBlock x:Name="PageSubtitle" Text="选择游戏与棋手，开始本地对局。" Foreground="{ThemeResource TextFillColorSecondaryBrush}" TextWrapping="Wrap"/>
      </StackPanel>
      <InfoBar x:Name="Notice" Grid.Row="1" IsOpen="False" IsClosable="True"/>
      <Grid x:Name="ArenaPage" Grid.Row="2" ColumnSpacing="16">
        <Grid.ColumnDefinitions>
          <ColumnDefinition Width="*"/>
          <ColumnDefinition Width="296"/>
        </Grid.ColumnDefinitions>
        <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="16">
          <Grid RowSpacing="12">
            <Grid.RowDefinitions>
              <RowDefinition Height="Auto"/>
              <RowDefinition Height="*"/>
              <RowDefinition Height="Auto"/>
              <RowDefinition Height="Auto"/>
              <RowDefinition Height="Auto"/>
            </Grid.RowDefinitions>
            <Grid ColumnSpacing="12">
              <Grid.ColumnDefinitions>
                <ColumnDefinition Width="Auto"/>
                <ColumnDefinition Width="*"/>
                <ColumnDefinition Width="Auto"/>
              </Grid.ColumnDefinitions>
              <ComboBox x:Name="GameSelector" SelectedIndex="0" Width="124" AutomationProperties.Name="对局游戏">
                <ComboBoxItem Content="五子棋"/>
                <ComboBoxItem Content="路墙棋"/>
              </ComboBox>
              <TextBlock x:Name="BoardDescription" Grid.Column="1" Text="15 × 15 · 自由规则" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource TextFillColorSecondaryBrush}" VerticalAlignment="Center" TextWrapping="Wrap"/>
              <TextBlock x:Name="MoveCounter" Grid.Column="2" Text="第 0 手" Style="{StaticResource CaptionTextBlockStyle}" VerticalAlignment="Center"/>
            </Grid>
            <Viewbox Grid.Row="1" Stretch="Uniform" HorizontalAlignment="Center" VerticalAlignment="Center">
              <Border Background="{ThemeResource LayerFillColorDefaultBrush}" CornerRadius="{ThemeResource OverlayCornerRadius}">
                <Canvas x:Name="BoardCanvas" Width="600" Height="600" Background="Transparent" AutomationProperties.Name="对局棋盘"/>
              </Border>
            </Viewbox>
            <StackPanel Grid.Row="2" Orientation="Horizontal" HorizontalAlignment="Center" Spacing="8" x:Name="WallTools" Visibility="Collapsed">
              <ToggleButton x:Name="PawnMode" Content="移动" IsChecked="True"/>
              <ToggleButton x:Name="HorizontalMode" Content="横墙"/>
              <ToggleButton x:Name="VerticalMode" Content="竖墙"/>
            </StackPanel>
            <CheckBox x:Name="ShowNumbers" Grid.Row="3" Content="显示手数"/>
            <TextBlock x:Name="BoardHint" Grid.Row="4" Text="选择棋手后开始对局" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource TextFillColorSecondaryBrush}" TextWrapping="Wrap"/>
          </Grid>
        </Border>
        <Grid Grid.Column="1" RowSpacing="16">
          <Grid.RowDefinitions>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="*"/>
          </Grid.RowDefinitions>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="20">
            <StackPanel Spacing="12">
              <TextBlock Text="新对局" Style="{StaticResource SubtitleTextBlockStyle}" AutomationProperties.HeadingLevel="Level2"/>
              <StackPanel Spacing="4">
                <TextBlock x:Name="FirstSeatLabel" Text="○  先手 · 白方" Style="{StaticResource BodyStrongTextBlockStyle}"/>
                <ComboBox x:Name="FirstPlayer" HorizontalAlignment="Stretch" AutomationProperties.LabeledBy="{Binding ElementName=FirstSeatLabel}"/>
              </StackPanel>
              <StackPanel Spacing="4">
                <TextBlock x:Name="SecondSeatLabel" Text="●  后手 · 黑方" Style="{StaticResource BodyStrongTextBlockStyle}"/>
                <ComboBox x:Name="SecondPlayer" HorizontalAlignment="Stretch" AutomationProperties.LabeledBy="{Binding ElementName=SecondSeatLabel}"/>
              </StackPanel>
              <Button x:Name="StartButton" Content="开始对局" Style="{StaticResource AccentButtonStyle}" HorizontalAlignment="Stretch"/>
              <Grid ColumnSpacing="8">
                <Grid.ColumnDefinitions>
                  <ColumnDefinition Width="*"/>
                  <ColumnDefinition Width="*"/>
                </Grid.ColumnDefinitions>
                <Button x:Name="UndoButton" Content="悔棋" HorizontalAlignment="Stretch" IsEnabled="False"/>
                <Button x:Name="StopButton" Content="结束对局" Grid.Column="1" HorizontalAlignment="Stretch" IsEnabled="False"/>
              </Grid>
              <Border Height="1" Background="{ThemeResource DividerStrokeColorDefaultBrush}" Margin="0,4"/>
              <StackPanel Orientation="Horizontal" Spacing="8">
                <ProgressRing x:Name="ThinkingRing" Width="20" Height="20" IsActive="False" Visibility="Collapsed"/>
                <TextBlock x:Name="MatchStatus" Text="等待开始" Style="{StaticResource BodyStrongTextBlockStyle}" AutomationProperties.LiveSetting="Polite"/>
              </StackPanel>
              <TextBlock x:Name="MatchDetail" Text="支持人机对战、程序互战与双人练习。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
            </StackPanel>
          </Border>
          <Border Grid.Row="1" Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="16">
            <Grid RowSpacing="8">
              <Grid.RowDefinitions>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="*"/>
                <RowDefinition Height="Auto"/>
              </Grid.RowDefinitions>
              <Grid>
                <TextBlock Text="行棋记录" Style="{StaticResource BodyStrongTextBlockStyle}"/>
                <TextBlock x:Name="LastTiming" Text="— ms" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource TextFillColorSecondaryBrush}" HorizontalAlignment="Right"/>
              </Grid>
              <ListView x:Name="MoveList" Grid.Row="1" SelectionMode="None" AutomationProperties.Name="行棋记录"/>
              <TextBlock x:Name="NoMoves" Grid.Row="1" Text="开始对局后，每一步会显示在这里。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" Margin="0,16,0,0" IsHitTestVisible="False"/>
              <TextBlock Grid.Row="2" Text="100 ms CPU / 步 · 256 MiB" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
            </Grid>
          </Border>
        </Grid>
      </Grid>
      <Grid x:Name="EditorPage" Grid.Row="2" Visibility="Collapsed" ColumnSpacing="16">
        <Grid.ColumnDefinitions>
          <ColumnDefinition Width="*"/>
          <ColumnDefinition Width="296"/>
        </Grid.ColumnDefinitions>
        <Grid RowSpacing="12">
          <Grid.RowDefinitions>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="*"/>
            <RowDefinition Height="Auto"/>
          </Grid.RowDefinitions>
          <Grid ColumnSpacing="12">
            <Grid.ColumnDefinitions>
              <ColumnDefinition Width="124"/>
              <ColumnDefinition Width="*"/>
            </Grid.ColumnDefinitions>
            <ComboBox x:Name="EditorGame" Header="游戏" HorizontalAlignment="Stretch" SelectedIndex="0">
              <ComboBoxItem Content="五子棋"/>
              <ComboBoxItem Content="路墙棋"/>
            </ComboBox>
            <TextBox x:Name="ProgramName" Grid.Column="1" Header="程序名称" PlaceholderText="为你的棋手命名" MaxLength="60"/>
          </Grid>
          <CommandBar Grid.Row="1" DefaultLabelPosition="Right" HorizontalAlignment="Stretch" OverflowButtonVisibility="Auto">
            <AppBarButton x:Name="LoadExample" Icon="Page" Label="载入示例"/>
            <AppBarButton x:Name="ImportSource" Icon="OpenFile" Label="导入源码"/>
            <AppBarButton x:Name="ExportSource" Icon="Save" Label="导出源码"/>
          </CommandBar>
          <TextBox x:Name="SourceEditor" Grid.Row="2" FontFamily="Consolas" FontSize="14" AcceptsReturn="True" TextWrapping="NoWrap" IsSpellCheckEnabled="False" ScrollViewer.HorizontalScrollBarVisibility="Auto" ScrollViewer.VerticalScrollBarVisibility="Auto" Padding="16" HorizontalAlignment="Stretch" VerticalAlignment="Stretch" AutomationProperties.Name="C++20 源码编辑器"/>
          <StackPanel Grid.Row="3" Spacing="4">
            <TextBlock x:Name="EditorStatus" Text="草稿自动保存" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
            <TextBlock Text="Ctrl+B 编译 · Ctrl+S 保存 · Tab 缩进" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource TextFillColorSecondaryBrush}" TextWrapping="Wrap"/>
          </StackPanel>
        </Grid>
        <Grid Grid.Column="1" RowSpacing="16">
          <Grid.RowDefinitions>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="*"/>
            <RowDefinition Height="*"/>
          </Grid.RowDefinitions>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="20">
            <StackPanel Spacing="12">
              <TextBlock Text="构建 Player" Style="{StaticResource SubtitleTextBlockStyle}"/>
              <TextBlock Text="将 C++20 源码编译为可参与对局的独立版本。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
              <Button x:Name="CompileButton" Content="编译并保存版本" Style="{StaticResource AccentButtonStyle}" HorizontalAlignment="Stretch"/>
              <ProgressBar x:Name="CompileProgress" IsIndeterminate="True" Visibility="Collapsed"/>
              <TextBlock Text="成功编译后，在对弈工作台选择该版本。" TextWrapping="Wrap" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
            </StackPanel>
          </Border>
          <Border Grid.Row="1" Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="16">
            <Grid RowSpacing="8">
              <Grid.RowDefinitions>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="*"/>
              </Grid.RowDefinitions>
              <TextBlock Text="已编译版本" Style="{StaticResource BodyStrongTextBlockStyle}"/>
              <ListView x:Name="ProgramList" Grid.Row="1" SelectionMode="Single" AutomationProperties.Name="已编译版本，选择后载入源码"/>
              <TextBlock x:Name="NoPrograms" Grid.Row="1" Text="还没有版本。载入示例，完成首次编译。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" Margin="0,16,0,0" IsHitTestVisible="False"/>
            </Grid>
          </Border>
          <Border Grid.Row="2" Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="16">
            <Grid RowSpacing="8">
              <Grid.RowDefinitions>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="*"/>
              </Grid.RowDefinitions>
              <TextBlock Text="编译与运行输出" Style="{StaticResource BodyStrongTextBlockStyle}"/>
              <TextBox x:Name="CompilerOutput" Grid.Row="1" IsReadOnly="True" AcceptsReturn="True" TextWrapping="Wrap" FontFamily="Consolas" FontSize="14" Text="编译结果与程序诊断会显示在这里。" AutomationProperties.Name="编译与运行输出"/>
            </Grid>
          </Border>
        </Grid>
      </Grid>
      <Grid x:Name="ReplayPage" Grid.Row="2" Visibility="Collapsed" ColumnSpacing="16">
        <Grid.ColumnDefinitions>
          <ColumnDefinition Width="256"/>
          <ColumnDefinition Width="*"/>
        </Grid.ColumnDefinitions>
        <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="16">
          <Grid RowSpacing="12">
            <Grid.RowDefinitions>
              <RowDefinition Height="Auto"/>
              <RowDefinition Height="*"/>
              <RowDefinition Height="Auto"/>
            </Grid.RowDefinitions>
            <TextBlock Text="对局档案" Style="{StaticResource SubtitleTextBlockStyle}"/>
            <ListView x:Name="ReplayList" Grid.Row="1" SelectionMode="Single" AutomationProperties.Name="对局档案"/>
            <TextBlock x:Name="NoReplays" Grid.Row="1" Text="还没有对局记录。&#xA;完成对局后会自动保存回放，也可以导入 JSON 文件。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" Margin="0,16,0,0" IsHitTestVisible="False"/>
            <Button Grid.Row="2" x:Name="ImportReplay" Content="导入回放" HorizontalAlignment="Stretch"/>
          </Grid>
        </Border>
        <Border Grid.Column="1" Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="16">
          <Grid RowSpacing="12">
            <Grid.RowDefinitions>
              <RowDefinition Height="Auto"/>
              <RowDefinition Height="*"/>
              <RowDefinition Height="Auto"/>
              <RowDefinition Height="Auto"/>
            </Grid.RowDefinitions>
            <StackPanel Spacing="4">
              <TextBlock x:Name="ReplayTitle" Text="选择一局回放" Style="{StaticResource SubtitleTextBlockStyle}" TextWrapping="Wrap"/>
              <TextBlock x:Name="ReplayDetail" Text="从对局档案中选择记录，逐步查看每次行动。" Foreground="{ThemeResource TextFillColorSecondaryBrush}" TextWrapping="Wrap"/>
            </StackPanel>
            <Viewbox Grid.Row="1" Stretch="Uniform">
              <Border Background="{ThemeResource LayerFillColorDefaultBrush}" CornerRadius="{ThemeResource OverlayCornerRadius}">
                <Canvas x:Name="ReplayCanvas" Width="600" Height="600" AutomationProperties.Name="回放棋盘"/>
              </Border>
            </Viewbox>
            <Slider x:Name="ReplaySlider" Grid.Row="2" Minimum="0" Maximum="1" Value="0" StepFrequency="1" IsEnabled="False" AutomationProperties.Name="回放手数"/>
            <Grid Grid.Row="3" ColumnSpacing="8">
              <Grid.ColumnDefinitions>
                <ColumnDefinition Width="*"/>
                <ColumnDefinition Width="Auto"/>
              </Grid.ColumnDefinitions>
              <StackPanel Orientation="Horizontal" Spacing="4">
                <Button x:Name="ReplayFirst" AutomationProperties.Name="回到开局" ToolTipService.ToolTip="回到开局">
                  <SymbolIcon Symbol="Previous"/>
                </Button>
                <Button x:Name="ReplayPrev" AutomationProperties.Name="上一步" ToolTipService.ToolTip="上一步">
                  <SymbolIcon Symbol="Back"/>
                </Button>
                <Button x:Name="ReplayPlay" Content="播放"/>
                <Button x:Name="ReplayNext" AutomationProperties.Name="下一步" ToolTipService.ToolTip="下一步">
                  <SymbolIcon Symbol="Forward"/>
                </Button>
                <Button x:Name="ReplayLast" AutomationProperties.Name="跳到终局" ToolTipService.ToolTip="跳到终局">
                  <SymbolIcon Symbol="Next"/>
                </Button>
                <TextBlock x:Name="ReplayStep" Text="0 / 0" Style="{StaticResource CaptionTextBlockStyle}" VerticalAlignment="Center" Margin="4,0"/>
              </StackPanel>
              <Button x:Name="ExportReplay" Grid.Column="1" Content="导出" IsEnabled="False"/>
            </Grid>
          </Grid>
        </Border>
      </Grid>
      <ScrollViewer x:Name="RulesPage" Grid.Row="2" Visibility="Collapsed" VerticalScrollBarVisibility="Auto">
        <StackPanel Spacing="16" MaxWidth="1040" HorizontalAlignment="Left">
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="24">
            <StackPanel Spacing="12">
              <TextBlock Text="Player 交互协议" Style="{StaticResource SubtitleTextBlockStyle}"/>
              <TextBlock Text="你的 C++ 程序是一位持续在线的棋手：整局只启动一次，通过标准输入接收对手行动，通过标准输出返回自己的一步。桌面端兼容 CompIntel 原版 v1 协议。" TextWrapping="Wrap" LineHeight="25" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
              <TextBlock Text="坐标从 0 开始，X 向右、Y 向下。每次输出必须换行并刷新；调试信息请写入 std::cerr。程序结束、超时、输出超限和非法行动会判负。" TextWrapping="Wrap" LineHeight="25" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
            </StackPanel>
          </Border>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="24">
            <StackPanel Spacing="12">
              <TextBlock Text="五子棋 · Gomoku" Style="{StaticResource SubtitleTextBlockStyle}"/>
              <TextBlock Text="15 × 15 棋盘，席位 0 执白先行，席位 1 执黑后行。横、竖或斜线连续至少五子即获胜，无禁手；棋盘填满则和棋。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" LineHeight="24"/>
              <TextBlock Text="启动输入                 行动输入 / 输出&#xA;1                         X Y&#xA;15 15&#xA;seat" FontFamily="Consolas" FontSize="14"/>
              <TextBlock Text="先手读完初始化后立即输出；后手先读取对手第一步。之后每次只接收对手刚走的一步。" TextWrapping="Wrap" Style="{StaticResource CaptionTextBlockStyle}"/>
              <Button x:Name="GomokuExample" Content="打开五子棋示例"/>
            </StackPanel>
          </Border>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="24">
            <StackPanel Spacing="12">
              <TextBlock Text="路墙棋 · Quoridor" Style="{StaticResource SubtitleTextBlockStyle}"/>
              <TextBlock Text="9 × 9 棋格，双方各有 10 面墙。席位 0 从 (4,0) 前往 Y=8，席位 1 从 (4,8) 前往 Y=0。每步移动棋子或放一面墙，率先抵达对岸任意棋格即获胜。200 手未分胜负则结束。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" LineHeight="24"/>
              <TextBlock Text="棋子可上下左右移动。紧邻对手且无墙时可直跳；仅在对手背后越界或被墙阻挡时允许侧向斜跳。墙不能交叉、重叠，或切断任意一方的所有通路。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" LineHeight="24"/>
              <TextBlock Text="启动输入：1 seat&#xA;移动棋子：0 X Y        棋格坐标 0..8&#xA;放置墙体：1 X Y O      墙中心坐标 1..8&#xA;O = 0 竖墙，O = 1 横墙" FontFamily="Consolas" FontSize="14"/>
              <TextBlock Text="在棋盘下方选择“横墙”或“竖墙”，点击四格之间的交叉点放置墙体。移动模式会标出当前所有合法落点。" TextWrapping="Wrap" Style="{StaticResource CaptionTextBlockStyle}"/>
              <Button x:Name="QuoridorExample" Content="打开路墙棋示例"/>
            </StackPanel>
          </Border>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="24">
            <StackPanel Spacing="10">
              <TextBlock Text="本地调试与资源限制" Style="{StaticResource SubtitleTextBlockStyle}"/>
              <TextBlock Text="默认每步 100 ms CPU、每方整局累计 5 s CPU、每步 1 s 墙上时间、256 MiB 进程内存、最多 8 个进程、每行输出 64 字节。等待对手时暂停程序，结束对局时关闭整个进程作业。墙上时间可在设置中调整。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" LineHeight="24"/>
              <TextBlock Text="本地程序使用当前用户权限运行。资源限制用于调试，不提供文件和网络安全隔离；请运行自己编写或信任的源码。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}" LineHeight="24"/>
            </StackPanel>
          </Border>
        </StackPanel>
      </ScrollViewer>
      <ScrollViewer x:Name="SettingsPage" Grid.Row="2" Visibility="Collapsed" VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Disabled" HorizontalContentAlignment="Stretch" HorizontalAlignment="Left" MaxWidth="1040">
        <StackPanel x:Name="SettingsContent" Spacing="16">
          <TextBlock Text="外观" Style="{StaticResource SubtitleTextBlockStyle}" AutomationProperties.HeadingLevel="Level2"/>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="20">
            <StackPanel Spacing="8">
              <ComboBox x:Name="ThemeSelector" Header="应用主题" SelectedIndex="0" MinWidth="240">
                <ComboBoxItem Content="跟随 Windows 设置"/>
                <ComboBoxItem Content="浅色"/>
                <ComboBoxItem Content="深色"/>
              </ComboBox>
              <TextBlock Text="主题立即预览；点击“保存设置”后记住选择。强调色使用 Windows 个性化设置。" Foreground="{ThemeResource TextFillColorSecondaryBrush}" TextWrapping="Wrap"/>
            </StackPanel>
          </Border>
          <TextBlock Text="编译与运行" Style="{StaticResource SubtitleTextBlockStyle}" AutomationProperties.HeadingLevel="Level2"/>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="20">
            <StackPanel Spacing="16">
              <TextBox x:Name="CompilerPath" Header="C++ 编译器路径" PlaceholderText="留空以自动检测 MSVC"/>
              <TextBlock Text="支持 MSVC、clang++ 和 g++，使用 C++20 编译。自动检测需要 Visual Studio C++ 生成工具。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
              <NumberBox x:Name="TurnTimeout" Header="每步墙上时间上限（毫秒）" Minimum="100" Maximum="10000" SmallChange="100" LargeChange="1000" SpinButtonPlacementMode="Inline" Value="1000" Width="260" HorizontalAlignment="Left"/>
              <Button x:Name="SaveSettings" Content="保存设置" Style="{StaticResource AccentButtonStyle}"/>
            </StackPanel>
          </Border>
          <TextBlock Text="本地数据" Style="{StaticResource SubtitleTextBlockStyle}" AutomationProperties.HeadingLevel="Level2"/>
          <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}" BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}" BorderThickness="1" CornerRadius="{ThemeResource OverlayCornerRadius}" Padding="20">
            <StackPanel Spacing="12">
              <TextBlock Text="草稿、程序版本、编译产物与回放均保存在这台电脑。" TextWrapping="Wrap" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
              <TextBlock x:Name="DataPath" IsTextSelectionEnabled="True" TextWrapping="Wrap"/>
              <Button x:Name="OpenData" Content="打开数据目录"/>
            </StackPanel>
          </Border>
          <TextBlock Text="CompIntel3 · 本地算法对弈工作台&#xA;兼容 CompIntel 游戏规则与 v1 交互协议。" Foreground="{ThemeResource TextFillColorSecondaryBrush}" Style="{StaticResource CaptionTextBlockStyle}" TextWrapping="Wrap" Margin="0,8,0,16"/>
        </StackPanel>
      </ScrollViewer>
    </Grid>
  </NavigationView>
  <!-- Live theme-resource sources for the programmatically drawn game surface.
      Bindings keep canvas primitives in sync with Light, Dark, accent and contrast changes. -->
  <StackPanel Visibility="Collapsed" IsHitTestVisible="False">
    <Rectangle x:Name="BoardLine" Fill="{ThemeResource TextFillColorTertiaryBrush}"/>
    <Rectangle x:Name="BoardText" Fill="{ThemeResource TextFillColorSecondaryBrush}"/>
    <Rectangle x:Name="BoardCell" Fill="{ThemeResource ControlFillColorDefaultBrush}"/>
    <Rectangle x:Name="BoardEdge" Fill="{ThemeResource ControlStrokeColorDefaultBrush}"/>
    <Rectangle x:Name="BoardAccent" Fill="{ThemeResource AccentFillColorDefaultBrush}"/>
    <Rectangle x:Name="BoardWhite">
      <Rectangle.Fill>
        <SolidColorBrush Color="{ThemeResource SystemColorWindowColor}"/>
      </Rectangle.Fill>
    </Rectangle>
    <Rectangle x:Name="BoardBlack">
      <Rectangle.Fill>
        <SolidColorBrush Color="{ThemeResource SystemColorWindowTextColor}"/>
      </Rectangle.Fill>
    </Rectangle>
  </StackPanel>
</Grid>
)XAML";
}
