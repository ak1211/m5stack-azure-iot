//
// テレメトリのCSVファイルから, プロットする
//
// Copyright (c) 2023 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
use anyhow::anyhow;
use chrono::{Duration, NaiveDateTime, NaiveTime, TimeZone, Timelike};
use chrono_tz::Asia::Tokyo;
use chrono_tz::Tz;
use clap::Parser;
use plotters::prelude::*;
use plotters::style::text_anchor::{HPos, Pos, VPos};
use polars::lazy::dsl::{Expr, StrptimeOptions};
use polars::lazy::prelude::*;
use polars::prelude::PolarsError::{ComputeError, NoData};
use polars::prelude::*;
use std::ffi::OsString;
use std::fs;
use std::fs::DirEntry;
use std::ops::Range;
use std::path::{Path, PathBuf};

mod colname {
    pub const MEASURED_AT: &'static str = "measuredAt";
    pub const SENSOR_ID: &'static str = "sensorId";
    pub const TEMPERATURE: &'static str = "temperature";
    pub const RELATIVE_HUMIDITY: &'static str = "humidity";
    pub const ABSOLUTE_HUMIDITY: &'static str = "absolute_humidity";
    pub const PRESSURE: &'static str = "pressure";
    pub const TVOC: &'static str = "tvoc";
    pub const ECO2: &'static str = "eCo2";
    pub const CO2: &'static str = "co2";
}

const COLOR_PALETTE: [RGBColor; 6] = [
    plotters::style::colors::full_palette::BLUE,
    plotters::style::colors::full_palette::ORANGE,
    plotters::style::colors::full_palette::GREEN,
    plotters::style::colors::full_palette::PINK,
    plotters::style::colors::full_palette::CYAN,
    plotters::style::colors::full_palette::DEEPORANGE,
];

// CSVファイルを読み込んでデータフレームを作る
fn read_csv<P: AsRef<Path>>(path: P) -> Result<LazyFrame, PolarsError> {
    let ldf = LazyCsvReader::new(path).has_header(true).finish()?;

    // measured_at列をstr型からdatetime型に変換する
    let expr: Expr = col(colname::MEASURED_AT)
        .str()
        .strptime(
            DataType::Datetime(TimeUnit::Milliseconds, None),
            StrptimeOptions {
                format: Some("%Y-%m-%dT%H:%M:%S%z".into()),
                strict: false,
                ..Default::default()
            },
        )
        .alias(colname::MEASURED_AT); // 変換済みの列で上書きする

    Ok(ldf.with_column(expr))
}

// X軸の日付時間
fn as_datetime_vector(
    series: &Series,
    tz: Tz,
) -> Result<(Vec<NaiveDateTime>, RangedDateTime<NaiveDateTime>), PolarsError> {
    // SeriesからNaiveDateTime(UTC)に変換する
    let original_datetimes = series
        .datetime()?
        .as_datetime_iter()
        .collect::<Option<Vec<NaiveDateTime>>>()
        .ok_or(ComputeError("datetime parse error".into()))?;
    // UTCからLocalに変換する
    let local_datetimes = original_datetimes
        .iter()
        .map(|t| tz.from_utc_datetime(t).naive_local())
        .collect::<Vec<NaiveDateTime>>();
    // 開始時間
    let start_datetime = local_datetimes
        .iter()
        .min()
        .ok_or(NoData("datetime".into()))?;
    // 終了時間
    let end_datetime = local_datetimes
        .iter()
        .max()
        .ok_or(NoData("datetime".into()))?;
    // 開始日の午前0時
    let day_start_datetime = NaiveDateTime::new(
        start_datetime.date(),
        NaiveTime::from_hms_opt(0, 0, 0).unwrap(),
    );
    // 終了日の翌日の午前0時
    let day_end_datetime = NaiveDateTime::new(
        end_datetime.date() + Duration::days(1),
        NaiveTime::from_hms_opt(0, 0, 0).unwrap(),
    );
    // 時間の範囲
    let range_datetime: Range<NaiveDateTime> = day_start_datetime..day_end_datetime;
    //
    Ok((local_datetimes, range_datetime.into()))
}

// 折れ線グラフを作る
fn line_chart<'a, DB: DrawingBackend>(
    chart: &mut ChartContext<
        'a,
        DB,
        Cartesian2d<RangedDateTime<NaiveDateTime>, plotters::coord::types::RangedCoordf64>,
    >,
    dataset: Vec<(&NaiveDateTime, &f64)>,
    sensor_id: &str,
    line_style: Box<ShapeStyle>,
) -> anyhow::Result<()>
where
    DB::ErrorType: 'static,
{
    // 点で表現する
    (*chart).draw_series(
        dataset
            .iter()
            .copied()
            .map(|(x, y)| Circle::new((*x, *y), 1, *line_style)),
    )?;
    // 折れ線で表現する
    (*chart)
        .draw_series(LineSeries::new(
            dataset
                .iter()
                .copied()
                .map(|(datetime, value)| (*datetime, *value)),
            *line_style,
        ))?
        .label(sensor_id)
        .legend(move |(x, y)| PathElement::new(vec![(x, y), (x + 20, y)], *line_style));

    Ok(())
}

// グラフを作る
fn plot<'a, DB: DrawingBackend>(
    area: &DrawingArea<DB, plotters::coord::Shift>,
    ldf: LazyFrame,
    column_name: &str,
    caption: &str,
    y_desc: &str,
    sensor_ids: &Vec<&str>,
) -> anyhow::Result<()>
where
    DB::ErrorType: 'static,
{
    let df = ldf
        .select([
            col(colname::SENSOR_ID),
            col(colname::MEASURED_AT),
            col(column_name),
        ])
        .filter(col(colname::SENSOR_ID).is_not_null())
        .filter(col(colname::MEASURED_AT).is_not_null())
        .filter(col(column_name).is_not_null())
        .collect()?;
    // X軸の日付時間
    let (_, range_datetime) = as_datetime_vector(&df[colname::MEASURED_AT], Tokyo)?;
    //
    let ymin = df[column_name]
        .f64()?
        .min()
        .ok_or(anyhow!("value is empty"))?;
    let ymax = df[column_name]
        .f64()?
        .max()
        .ok_or(anyhow!("value is empty"))?;
    //
    let mut chart = ChartBuilder::on(area)
        .caption(caption, ("sans-serif", 16).into_font())
        .margin(10)
        .x_label_area_size(70)
        .y_label_area_size(70)
        .build_cartesian_2d(range_datetime.clone(), ymin..ymax)?;
    //
    let custom_x_label_formatter = |t: &NaiveDateTime| {
        if t.time().hour() == 0 {
            t.format("%Y-%m-%d %a").to_string()
        } else {
            t.format("%H:%M:%S").to_string()
        }
    };
    // 軸ラベルとか
    chart
        .configure_mesh()
        .x_labels(24)
        .x_label_style(
            ("sans-serif", 11)
                .into_font()
                .transform(FontTransform::Rotate270)
                .with_anchor::<RGBColor>(Pos::new(HPos::Right, VPos::Top)),
        )
        .x_label_formatter(&custom_x_label_formatter)
        .set_tick_mark_size(LabelAreaPosition::Bottom, 20)
        .y_desc(y_desc)
        .draw()?;
    //
    for (index, sensor_id) in sensor_ids.iter().enumerate() {
        // センサーID毎のデーターフレーム
        let sensor_df = df
            .clone()
            .lazy()
            .filter(col(colname::SENSOR_ID).eq(lit(*sensor_id)))
            .collect()?;
        //
        if sensor_df[0].is_empty() {
            continue;
        }
        // X軸の日付時間
        let (datetimes, _) = as_datetime_vector(&sensor_df[colname::MEASURED_AT], Tokyo)?;
        // Y軸の測定値
        let values = sensor_df[column_name]
            .iter()
            .map(|x| x.try_extract())
            .collect::<Result<Vec<f64>, _>>()?;
        //
        line_chart(
            &mut chart,
            itertools::izip!(&datetimes, &values).collect(),
            sensor_id,
            Box::new(COLOR_PALETTE.get(index).unwrap_or(&COLOR_PALETTE[0]).into()),
        )?;
    }
    // 凡例
    chart
        .configure_series_labels()
        .background_style(WHITE.mix(0.5))
        .border_style(BLACK)
        .draw()?;

    Ok(())
}

// グラフを作る
fn plot_dataframe<DB: DrawingBackend>(
    root_area: DrawingArea<DB, plotters::coord::Shift>,
    ldf: LazyFrame,
) -> anyhow::Result<()>
where
    DB::ErrorType: 'static,
{
    // センサーIDを取り出す
    let sensor_id_df = ldf
        .clone()
        .groupby([col(colname::SENSOR_ID)])
        .agg([])
        .collect()?;
    // センサーIDの種類
    let sensor_ids: Vec<&str> = sensor_id_df[0].utf8()?.into_no_null_iter().collect();

    // 背景色
    root_area.fill(&WHITE)?;
    // 縦に4分割する
    // 横に2分割する
    let areas = root_area.split_evenly((4, 2));
    if let [one_l, one_r, two_l, two_r, three_l, three_r, four_l, four_r] = &areas[..8] {
        // 気温グラフを作る
        plot(
            one_l,
            ldf.clone(),
            colname::TEMPERATURE,
            "temperature",
            "C",
            &sensor_ids,
        )?;
        // 気圧グラフを作る
        plot(
            one_r,
            ldf.clone(),
            colname::PRESSURE,
            "pressure",
            "hPa",
            &sensor_ids,
        )?;
        // 相対湿度グラフを作る
        plot(
            two_l,
            ldf.clone(),
            colname::RELATIVE_HUMIDITY,
            "relative humidity",
            "%RH",
            &sensor_ids,
        )?;
        // 絶対湿度グラフを作る
        plot(
            two_r,
            ldf.clone(),
            colname::ABSOLUTE_HUMIDITY,
            "absolute humidity",
            "mg/m^3",
            &sensor_ids,
        )?;
        // 二酸化炭素濃度グラフを作る
        plot(
            three_l,
            ldf.clone(),
            colname::CO2,
            "carbon dioxide (CO2)",
            "ppm",
            &sensor_ids,
        )?;
        // Total VOCグラフを作る
        plot(
            three_r,
            ldf.clone(),
            colname::TVOC,
            "Total VOC",
            "ppb",
            &sensor_ids,
        )?;
        // 二酸化炭素相当量グラフを作る
        plot(
            four_l,
            ldf,
            colname::ECO2,
            "equivalent CO2",
            "ppm",
            &sensor_ids,
        )?;
    } else {
        panic!("fatal error")
    }
    // プロット完了
    root_area.present()?;

    Ok(())
}

#[derive(Debug, Copy, Clone, PartialEq)]
enum ChartFileType {
    Png,
    Svg,
}

// csvファイルからグラフを作る
fn run<P: AsRef<Path>>(
    infilepath: P,
    overwrite: bool,
    plotareasize: (u32, u32),
    chart_file_type: ChartFileType,
) -> anyhow::Result<String> {
    // 出力するファイル名は入力ファイルの.csvを.png/.svgに変えたもの
    let infilepath_string = format!("{:?}", infilepath.as_ref().as_os_str());
    let mut outfilepath: PathBuf = PathBuf::from(infilepath.as_ref());
    outfilepath.set_extension(match chart_file_type {
        ChartFileType::Png => "png",
        ChartFileType::Svg => "svg",
    });
    // 出力するファイルの存在確認
    if outfilepath.is_file() && !overwrite {
        let outfilepath_string = format!("{:?}", outfilepath.as_os_str());
        Err(anyhow!("{} file is already exist!", outfilepath_string))?;
    }
    // CSVファイルからデーターフレームを作る
    let df: DataFrame = read_csv(infilepath)?
        .sort(colname::MEASURED_AT, SortOptions::default())
        .collect()?;
    //
    match chart_file_type {
        ChartFileType::Png => {
            let root_area = BitMapBackend::new(&outfilepath, plotareasize).into_drawing_area();
            plot_dataframe(root_area, df.clone().lazy())?;
        }
        ChartFileType::Svg => {
            let root_area = SVGBackend::new(&outfilepath, plotareasize).into_drawing_area();
            plot_dataframe(root_area, df.clone().lazy())?;
        }
    };
    // 結果を返す
    Ok(format!("inputfile -> {}\n{:?}", infilepath_string, df))
}

#[derive(Parser)]
#[command(author, version, about, about ="Plot chart of telemetry data", long_about = None)] // Read from `Cargo.toml`
struct Cli {
    #[arg(short = 'x', long, default_value_t = 1000)]
    width: u32,
    #[arg(short = 'y', long, default_value_t = 1000)]
    height: u32,
    #[arg(long)]
    png: bool,
    #[arg(long)]
    overwrite: bool,
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    //
    // カレントディレクトリ
    let dir = fs::read_dir("./")?;
    // ディレクトリのファイルリスト
    let files: Vec<DirEntry> = dir.into_iter().collect::<Result<Vec<DirEntry>, _>>()?;
    // サフィックスが"csv"のファイルリスト
    let csv_files: Vec<&DirEntry> = files
        .iter()
        .filter(|s: &&DirEntry| {
            let suffix = OsString::from("csv").to_ascii_lowercase();
            s.path().extension().map(|a| a.to_ascii_lowercase()) == Some(suffix)
        })
        .collect();
    // 出力ファイルの種類
    let chart_file_type = if cli.png {
        ChartFileType::Png
    } else {
        ChartFileType::Svg
    };
    // グラフの大きさ
    let plotareasize = (cli.width, cli.height);
    // csvファイルからグラフを作る
    for p in csv_files {
        let result = run(p.path(), cli.overwrite, plotareasize, chart_file_type)
            .unwrap_or_else(|e| format!("{:?}", e));
        println!("{}", result);
    }

    Ok(())
}
