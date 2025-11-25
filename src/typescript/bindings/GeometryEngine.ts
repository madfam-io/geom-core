/**
 * GeometryEngine - Unified OCCT Geometry Engine
 *
 * Consolidated from sim4d/engine-occt bindings for cross-project use.
 * Provides a single entry point for all geometry operations.
 *
 * @module geom-core
 */

import type {
  WASMModule,
  OCCTShape,
  OCCTHandle,
  OCCTVec3,
  ShapeHandle,
  Vec3,
  BoundingBox,
  MeshData,
  TessellateOptions,
  BoxParams,
  SphereParams,
  CylinderParams,
  ConeParams,
  TorusParams,
  LineParams,
  CircleParams,
  RectangleParams,
  ArcParams,
  PolygonParams,
  EllipseParams,
  BooleanUnionParams,
  BooleanSubtractParams,
  BooleanIntersectParams,
  ExtrudeParams,
  RevolveParams,
  SweepParams,
  LoftParams,
  FilletParams,
  ChamferParams,
  ShellParams,
  DraftParams,
  OffsetParams,
  TransformParams,
  TranslateParams,
  RotateParams,
  ScaleParams,
  MirrorParams,
  ShapeProperties,
  OperationResult,
  ComplexityEstimate,
  ComputeLocation,
} from './types';

import { createHandleId, getShapeId } from './types';

// =============================================================================
// Logger
// =============================================================================

type LogLevel = 'debug' | 'info' | 'warn' | 'error';

interface Logger {
  debug(msg: string, ...args: unknown[]): void;
  info(msg: string, ...args: unknown[]): void;
  warn(msg: string, ...args: unknown[]): void;
  error(msg: string, ...args: unknown[]): void;
}

function createLogger(module: string): Logger {
  const log = (level: LogLevel, msg: string, ...args: unknown[]) => {
    if (process.env.NODE_ENV !== 'production' || level === 'error') {
      console[level](`[${module}]`, msg, ...args);
    }
  };

  return {
    debug: (msg, ...args) => log('debug', msg, ...args),
    info: (msg, ...args) => log('info', msg, ...args),
    warn: (msg, ...args) => log('warn', msg, ...args),
    error: (msg, ...args) => log('error', msg, ...args),
  };
}

// =============================================================================
// ID Generator
// =============================================================================

class IDGenerator {
  private nextId: number;

  constructor(startId: number = 1) {
    this.nextId = startId;
  }

  generate(): string {
    return `shape_${this.nextId++}`;
  }

  getCurrent(): number {
    return this.nextId;
  }

  setCurrent(id: number): void {
    this.nextId = id;
  }
}

// =============================================================================
// Vec3 Utilities
// =============================================================================

class Vec3Utils {
  constructor(private occt: WASMModule) {}

  createVec3(v: Vec3): OCCTVec3 {
    const vec = new this.occt.gp_Vec();
    vec.SetCoord(v.x, v.y, v.z);
    return vec;
  }

  createPoint(v: Vec3): OCCTHandle {
    return new this.occt.gp_Pnt(v.x, v.y, v.z);
  }

  createDir(v: Vec3): OCCTHandle {
    return new this.occt.gp_Dir(v.x, v.y, v.z);
  }

  createAxis(center: Vec3, direction: Vec3): OCCTHandle {
    const pnt = this.createPoint(center);
    const dir = this.createDir(direction);
    return new this.occt.gp_Ax2(pnt, dir);
  }
}

// =============================================================================
// Shape Handle Utilities
// =============================================================================

class ShapeUtils {
  constructor(
    private occt: WASMModule,
    private shapes: Map<string, OCCTShape>,
    private idGen: IDGenerator
  ) {}

  calculateBounds(shape: OCCTShape): BoundingBox {
    const bnd = new this.occt.Bnd_Box();
    const builder = new this.occt.BRepBndLib();
    builder.Add(shape, bnd);

    const min = bnd.CornerMin();
    const max = bnd.CornerMax();

    const bbox: BoundingBox = {
      min: { x: min.X!(), y: min.Y!(), z: min.Z!() },
      max: { x: max.X!(), y: max.Y!(), z: max.Z!() },
    };

    min.delete();
    max.delete();
    bnd.delete();
    builder.delete();

    return bbox;
  }

  createHandle(shape: OCCTShape, type: ShapeHandle['type'] = 'solid'): ShapeHandle {
    const id = this.idGen.generate();
    const bbox = this.calculateBounds(shape);

    this.shapes.set(id, shape);

    return {
      id: createHandleId(id),
      type,
      bbox,
      hash: id.substring(0, 16),
    };
  }

  getShape(shapeOrId: string | ShapeHandle): OCCTShape | undefined {
    const id = getShapeId(shapeOrId);
    return this.shapes.get(id);
  }
}

// =============================================================================
// Operation Timing Decorator
// =============================================================================

function timed<T>(
  operation: string,
  fn: () => T,
  logger: Logger
): OperationResult<T> {
  const start = performance.now();

  try {
    const value = fn();
    const durationMs = performance.now() - start;

    if (durationMs > 100) {
      logger.warn(`Slow operation: ${operation} took ${durationMs.toFixed(2)}ms`);
    }

    return {
      success: true,
      value,
      durationMs,
    };
  } catch (error) {
    const durationMs = performance.now() - start;
    const message = error instanceof Error ? error.message : String(error);

    logger.error(`Operation failed: ${operation}`, error);

    return {
      success: false,
      error: {
        code: 'OPERATION_FAILED',
        message,
      },
      durationMs,
    };
  }
}

// =============================================================================
// GeometryEngine Class
// =============================================================================

export class GeometryEngine {
  private occt: WASMModule;
  private shapes: Map<string, OCCTShape>;
  private idGen: IDGenerator;
  private vec3: Vec3Utils;
  private shapeUtils: ShapeUtils;
  private logger: Logger;
  private initialized = false;

  constructor(occt: WASMModule) {
    this.occt = occt;
    this.shapes = new Map();
    this.idGen = new IDGenerator();
    this.vec3 = new Vec3Utils(occt);
    this.shapeUtils = new ShapeUtils(occt, this.shapes, this.idGen);
    this.logger = createLogger('GeometryEngine');
    this.initialized = true;
  }

  // ===========================================================================
  // Lifecycle
  // ===========================================================================

  isInitialized(): boolean {
    return this.initialized;
  }

  getShapeCount(): number {
    return this.shapes.size;
  }

  getShape(id: string): OCCTShape | undefined {
    return this.shapes.get(id);
  }

  disposeShape(id: string): boolean {
    const shape = this.shapes.get(id);
    if (shape) {
      shape.delete();
      this.shapes.delete(id);
      return true;
    }
    return false;
  }

  disposeAll(): void {
    for (const shape of this.shapes.values()) {
      shape.delete();
    }
    this.shapes.clear();
    this.idGen.setCurrent(1);
  }

  // ===========================================================================
  // 3D Primitives
  // ===========================================================================

  makeBox(params: BoxParams = {}): OperationResult<ShapeHandle> {
    return timed('makeBox', () => {
      const { width = 100, height = 100, depth = 100 } = params;

      const builder = new this.occt.BRepPrimAPI_MakeBox(width, height, depth);
      const shape = builder.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      builder.delete();
      return handle;
    }, this.logger);
  }

  makeSphere(params: SphereParams = {}): OperationResult<ShapeHandle> {
    return timed('makeSphere', () => {
      const { radius = 50, center = { x: 0, y: 0, z: 0 } } = params;

      const centerPnt = this.vec3.createPoint(center);
      const builder = new this.occt.BRepPrimAPI_MakeSphere(centerPnt, radius);
      const shape = builder.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      centerPnt.delete();
      builder.delete();
      return handle;
    }, this.logger);
  }

  makeCylinder(params: CylinderParams = {}): OperationResult<ShapeHandle> {
    return timed('makeCylinder', () => {
      const {
        radius = 50,
        height = 100,
        center = { x: 0, y: 0, z: 0 },
        axis = { x: 0, y: 0, z: 1 }
      } = params;

      const ax = this.vec3.createAxis(center, axis);
      const builder = new this.occt.BRepPrimAPI_MakeCylinder(ax, radius, height);
      const shape = builder.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      ax.delete();
      builder.delete();
      return handle;
    }, this.logger);
  }

  makeCone(params: ConeParams = {}): OperationResult<ShapeHandle> {
    return timed('makeCone', () => {
      const {
        radius1 = 50,
        radius2 = 25,
        height = 100,
        center = { x: 0, y: 0, z: 0 },
        axis = { x: 0, y: 0, z: 1 }
      } = params;

      const ax = this.vec3.createAxis(center, axis);
      const builder = new this.occt.BRepPrimAPI_MakeCone(ax, radius1, radius2, height);
      const shape = builder.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      ax.delete();
      builder.delete();
      return handle;
    }, this.logger);
  }

  makeTorus(params: TorusParams = {}): OperationResult<ShapeHandle> {
    return timed('makeTorus', () => {
      const {
        majorRadius = 50,
        minorRadius = 20,
        center = { x: 0, y: 0, z: 0 },
        axis = { x: 0, y: 0, z: 1 }
      } = params;

      const ax = this.vec3.createAxis(center, axis);
      const builder = new this.occt.BRepPrimAPI_MakeTorus(ax, majorRadius, minorRadius);
      const shape = builder.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      ax.delete();
      builder.delete();
      return handle;
    }, this.logger);
  }

  // ===========================================================================
  // 2D Primitives
  // ===========================================================================

  createLine(params: LineParams): OperationResult<ShapeHandle> {
    return timed('createLine', () => {
      const start = this.vec3.createPoint(params.start);
      const end = this.vec3.createPoint(params.end);

      const edge = new this.occt.BRepBuilderAPI_MakeEdge(start, end);
      const shape = edge.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'curve');

      start.delete();
      end.delete();
      edge.delete();
      return handle;
    }, this.logger);
  }

  createCircle(params: CircleParams = {}): OperationResult<ShapeHandle> {
    return timed('createCircle', () => {
      const {
        center = { x: 0, y: 0, z: 0 },
        radius = 50,
        normal = { x: 0, y: 0, z: 1 }
      } = params;

      const centerPnt = this.vec3.createPoint(center);
      const normalDir = this.vec3.createDir(normal);
      const axis = new this.occt.gp_Ax2(centerPnt, normalDir);

      const circle = new this.occt.gp_Circ(axis, radius);
      const edge = new this.occt.BRepBuilderAPI_MakeEdge(circle as unknown as OCCTHandle, circle as unknown as OCCTHandle);
      const wire = new this.occt.BRepBuilderAPI_MakeWire();
      wire.Add(edge.Edge!());

      const shape = wire.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'wire');

      centerPnt.delete();
      normalDir.delete();
      axis.delete();
      circle.delete();
      edge.delete();
      wire.delete();
      return handle;
    }, this.logger);
  }

  createRectangle(params: RectangleParams = {}): OperationResult<ShapeHandle> {
    return timed('createRectangle', () => {
      const {
        center = { x: 0, y: 0, z: 0 },
        width = 100,
        height = 100
      } = params;

      const p1 = this.vec3.createPoint({ x: center.x - width/2, y: center.y - height/2, z: center.z });
      const p2 = this.vec3.createPoint({ x: center.x + width/2, y: center.y - height/2, z: center.z });
      const p3 = this.vec3.createPoint({ x: center.x + width/2, y: center.y + height/2, z: center.z });
      const p4 = this.vec3.createPoint({ x: center.x - width/2, y: center.y + height/2, z: center.z });

      const wire = new this.occt.BRepBuilderAPI_MakeWire();

      const e1 = new this.occt.BRepBuilderAPI_MakeEdge(p1, p2);
      const e2 = new this.occt.BRepBuilderAPI_MakeEdge(p2, p3);
      const e3 = new this.occt.BRepBuilderAPI_MakeEdge(p3, p4);
      const e4 = new this.occt.BRepBuilderAPI_MakeEdge(p4, p1);

      wire.Add(e1.Edge!());
      wire.Add(e2.Edge!());
      wire.Add(e3.Edge!());
      wire.Add(e4.Edge!());

      const shape = wire.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'wire');

      p1.delete(); p2.delete(); p3.delete(); p4.delete();
      e1.delete(); e2.delete(); e3.delete(); e4.delete();
      wire.delete();
      return handle;
    }, this.logger);
  }

  createPolygon(params: PolygonParams): OperationResult<ShapeHandle> {
    return timed('createPolygon', () => {
      const { points, closed = true } = params;

      if (points.length < 3) {
        throw new Error('Polygon requires at least 3 points');
      }

      const wire = new this.occt.BRepBuilderAPI_MakeWire();

      for (let i = 0; i < points.length; i++) {
        const current = points[i];
        const next = points[(i + 1) % points.length];

        if (i === points.length - 1 && !closed) break;

        const p1 = new this.occt.gp_Pnt(current.x, current.y, current.z || 0);
        const p2 = new this.occt.gp_Pnt(next.x, next.y, next.z || 0);

        const line = new this.occt.GC_MakeSegment(p1, p2);
        if (line.IsDone()) {
          const edge = new this.occt.BRepBuilderAPI_MakeEdge(line.Value(), line.Value());
          edge.Build();
          if (edge.IsDone()) {
            wire.Add(edge.Shape());
          }
          edge.delete();
        }
        line.delete();
        p1.delete();
        p2.delete();
      }

      wire.Build();
      if (!wire.IsDone()) {
        throw new Error('Failed to create polygon');
      }

      const shape = wire.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'wire');

      wire.delete();
      return handle;
    }, this.logger);
  }

  // ===========================================================================
  // Boolean Operations
  // ===========================================================================

  booleanUnion(params: BooleanUnionParams): OperationResult<ShapeHandle> {
    return timed('booleanUnion', () => {
      const { shapes } = params;

      if (shapes.length < 2) {
        throw new Error('Union requires at least 2 shapes');
      }

      let result = this.shapeUtils.getShape(shapes[0]);
      if (!result) throw new Error('First shape not found');

      for (let i = 1; i < shapes.length; i++) {
        const tool = this.shapeUtils.getShape(shapes[i]);
        if (!tool) continue;

        const fuse = new this.occt.BRepAlgoAPI_Fuse(result, tool);
        fuse.Build();

        if (i > 1 && result) {
          result.delete();
        }

        const newResult = fuse.Shape();
        fuse.delete();

        if (!newResult) throw new Error('Boolean union operation failed');
        result = newResult;
      }

      if (!result) throw new Error('Boolean union: final result is null');
      return this.shapeUtils.createHandle(result, 'solid');
    }, this.logger);
  }

  booleanSubtract(params: BooleanSubtractParams): OperationResult<ShapeHandle> {
    return timed('booleanSubtract', () => {
      const base = this.shapeUtils.getShape(params.base);
      const { tools } = params;

      if (!base) throw new Error('Base shape not found');
      if (tools.length === 0) throw new Error('Subtract requires at least one tool');

      let result = base;

      for (const toolRef of tools) {
        const tool = this.shapeUtils.getShape(toolRef);
        if (!tool) continue;

        const cut = new this.occt.BRepAlgoAPI_Cut(result, tool);
        cut.Build();

        if (result !== base) {
          result.delete();
        }

        result = cut.Shape();
        cut.delete();
      }

      return this.shapeUtils.createHandle(result, 'solid');
    }, this.logger);
  }

  booleanIntersect(params: BooleanIntersectParams): OperationResult<ShapeHandle> {
    return timed('booleanIntersect', () => {
      const { shapes } = params;

      if (shapes.length < 2) {
        throw new Error('Intersect requires at least 2 shapes');
      }

      let result = this.shapeUtils.getShape(shapes[0]);
      if (!result) throw new Error('First shape not found');

      for (let i = 1; i < shapes.length; i++) {
        const tool = this.shapeUtils.getShape(shapes[i]);
        if (!tool) continue;

        const common = new this.occt.BRepAlgoAPI_Common(result, tool);
        common.Build();

        if (i > 1 && result) {
          result.delete();
        }

        const newResult = common.Shape();
        common.delete();

        if (!newResult) throw new Error('Boolean intersection operation failed');
        result = newResult;
      }

      if (!result) throw new Error('Boolean intersection: final result is null');
      return this.shapeUtils.createHandle(result, 'solid');
    }, this.logger);
  }

  // ===========================================================================
  // Feature Operations
  // ===========================================================================

  extrude(params: ExtrudeParams): OperationResult<ShapeHandle> {
    return timed('extrude', () => {
      const profile = this.shapeUtils.getShape(params.profile);
      if (!profile) throw new Error('Profile shape not found');

      const { direction = { x: 0, y: 0, z: 1 }, distance = 100 } = params;

      const vec = this.vec3.createVec3({
        x: direction.x * distance,
        y: direction.y * distance,
        z: direction.z * distance,
      });

      const prism = new this.occt.BRepPrimAPI_MakePrism(profile, vec);
      const shape = prism.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      vec.delete();
      prism.delete();
      return handle;
    }, this.logger);
  }

  revolve(params: RevolveParams): OperationResult<ShapeHandle> {
    return timed('revolve', () => {
      const profile = this.shapeUtils.getShape(params.profile);
      if (!profile) throw new Error('Profile shape not found');

      const {
        center = { x: 0, y: 0, z: 0 },
        axis = { x: 0, y: 0, z: 1 },
        angle = Math.PI * 2
      } = params;

      const ax = this.vec3.createAxis(center, axis);
      const revolve = new this.occt.BRepPrimAPI_MakeRevol(profile, ax, angle);

      const shape = revolve.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      ax.delete();
      revolve.delete();
      return handle;
    }, this.logger);
  }

  sweep(params: SweepParams): OperationResult<ShapeHandle> {
    return timed('sweep', () => {
      const profile = this.shapeUtils.getShape(params.profile);
      const path = this.shapeUtils.getShape(params.path);

      if (!profile || !path) throw new Error('Profile or path shape not found');

      const sweep = new this.occt.BRepOffsetAPI_MakePipe(path, profile);
      const shape = sweep.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      sweep.delete();
      return handle;
    }, this.logger);
  }

  loft(params: LoftParams): OperationResult<ShapeHandle> {
    return timed('loft', () => {
      const { profiles, solid = true } = params;

      if (profiles.length < 2) throw new Error('Loft requires at least 2 profiles');

      const loft = new this.occt.BRepOffsetAPI_ThruSections(solid);

      for (const profileRef of profiles) {
        const profile = this.shapeUtils.getShape(profileRef);
        if (profile) {
          loft.AddWire(profile);
        }
      }

      loft.Build();
      const shape = loft.Shape();
      const handle = this.shapeUtils.createHandle(shape, 'solid');

      loft.delete();
      return handle;
    }, this.logger);
  }

  fillet(params: FilletParams): OperationResult<ShapeHandle> {
    return timed('fillet', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const { radius = 5 } = params;

      const fillet = new this.occt.BRepFilletAPI_MakeFillet(shape);

      const explorer = new this.occt.TopExp_Explorer(shape, this.occt.TopAbs_EDGE);
      while (explorer.More()) {
        fillet.Add(radius, explorer.Current());
        explorer.Next();
      }

      fillet.Build();
      const result = fillet.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      explorer.delete();
      fillet.delete();
      return handle;
    }, this.logger);
  }

  chamfer(params: ChamferParams): OperationResult<ShapeHandle> {
    return timed('chamfer', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const { distance = 5 } = params;

      const chamfer = new this.occt.BRepFilletAPI_MakeChamfer(shape);

      const explorer = new this.occt.TopExp_Explorer(shape, this.occt.TopAbs_EDGE);
      while (explorer.More()) {
        chamfer.Add(distance, explorer.Current());
        explorer.Next();
      }

      chamfer.Build();
      const result = chamfer.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      explorer.delete();
      chamfer.delete();
      return handle;
    }, this.logger);
  }

  shell(params: ShellParams): OperationResult<ShapeHandle> {
    return timed('shell', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const { thickness = -5 } = params;

      const faces = new this.occt.TopTools_ListOfShape();

      const explorer = new this.occt.TopExp_Explorer(shape, this.occt.TopAbs_FACE);
      if (explorer.More()) {
        faces.Append(explorer.Current());
      }

      const shell = new this.occt.BRepOffsetAPI_MakeThickSolid();
      shell.MakeThickSolidByJoin(shape, faces, thickness, 1.0e-3);

      const result = shell.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      faces.delete();
      explorer.delete();
      shell.delete();
      return handle;
    }, this.logger);
  }

  offset(params: OffsetParams): OperationResult<ShapeHandle> {
    return timed('offset', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const { distance = 1.0 } = params;

      const offset = new this.occt.BRepOffsetAPI_MakeOffsetShape();
      offset.PerformByJoin(shape, distance, 1e-7);

      if (!offset.IsDone()) {
        offset.delete();
        throw new Error('Failed to create offset shape');
      }

      const result = offset.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      offset.delete();
      return handle;
    }, this.logger);
  }

  // ===========================================================================
  // Transformations
  // ===========================================================================

  transform(params: TransformParams): OperationResult<ShapeHandle> {
    return timed('transform', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const trsf = new this.occt.gp_Trsf();

      const matrix = Array.isArray(params.matrix)
        ? params.matrix
        : [
            params.matrix.m11 || 1, params.matrix.m12 || 0, params.matrix.m13 || 0, params.matrix.m14 || 0,
            params.matrix.m21 || 0, params.matrix.m22 || 1, params.matrix.m23 || 0, params.matrix.m24 || 0,
            params.matrix.m31 || 0, params.matrix.m32 || 0, params.matrix.m33 || 1, params.matrix.m34 || 0,
            params.matrix.m41 || 0, params.matrix.m42 || 0, params.matrix.m43 || 0, params.matrix.m44 || 1,
          ];

      // OCCT uses 3x4 transformation matrix
      (trsf as any).SetValues(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11]
      );

      const transformer = new this.occt.BRepBuilderAPI_Transform(shape, trsf, true);
      const result = transformer.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      trsf.delete();
      transformer.delete();
      return handle;
    }, this.logger);
  }

  translate(params: TranslateParams): OperationResult<ShapeHandle> {
    return timed('translate', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const vec = this.vec3.createVec3(params.vector);
      const trsf = new this.occt.gp_Trsf();
      (trsf as any).SetTranslation(vec);

      const transformer = new this.occt.BRepBuilderAPI_Transform(shape, trsf, true);
      const result = transformer.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      vec.delete();
      trsf.delete();
      transformer.delete();
      return handle;
    }, this.logger);
  }

  rotate(params: RotateParams): OperationResult<ShapeHandle> {
    return timed('rotate', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const { center = { x: 0, y: 0, z: 0 }, axis = { x: 0, y: 0, z: 1 }, angle } = params;

      const ax = this.vec3.createAxis(center, axis);
      const trsf = new this.occt.gp_Trsf();
      (trsf as any).SetRotation(ax.Axis!(), angle);

      const transformer = new this.occt.BRepBuilderAPI_Transform(shape, trsf, true);
      const result = transformer.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      ax.delete();
      trsf.delete();
      transformer.delete();
      return handle;
    }, this.logger);
  }

  scale(params: ScaleParams): OperationResult<ShapeHandle> {
    return timed('scale', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const { center = { x: 0, y: 0, z: 0 }, factor } = params;

      const centerPnt = this.vec3.createPoint(center);
      const trsf = new this.occt.gp_Trsf();
      (trsf as any).SetScale(centerPnt, factor);

      const transformer = new this.occt.BRepBuilderAPI_Transform(shape, trsf, true);
      const result = transformer.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      centerPnt.delete();
      trsf.delete();
      transformer.delete();
      return handle;
    }, this.logger);
  }

  mirror(params: MirrorParams): OperationResult<ShapeHandle> {
    return timed('mirror', () => {
      const shape = this.shapeUtils.getShape(params.shape);
      if (!shape) throw new Error('Shape not found');

      const { point = { x: 0, y: 0, z: 0 }, normal = { x: 0, y: 0, z: 1 } } = params;

      const pointPnt = this.vec3.createPoint(point);
      const normalDir = this.vec3.createDir(normal);
      const plane = new this.occt.gp_Ax2(pointPnt, normalDir);

      const trsf = new this.occt.gp_Trsf();
      (trsf as any).SetMirror(plane.Ax2!());

      const transformer = new this.occt.BRepBuilderAPI_Transform(shape, trsf, true);
      const result = transformer.Shape();
      const handle = this.shapeUtils.createHandle(result, 'solid');

      pointPnt.delete();
      normalDir.delete();
      plane.delete();
      trsf.delete();
      transformer.delete();
      return handle;
    }, this.logger);
  }

  // ===========================================================================
  // Analysis
  // ===========================================================================

  tessellate(shapeOrId: string | ShapeHandle, options: TessellateOptions = {}): OperationResult<MeshData> {
    return timed('tessellate', () => {
      const shape = this.shapeUtils.getShape(shapeOrId);
      if (!shape) throw new Error('Shape not found');

      const { deflection = 0.1, angle = 0.5 } = options;

      const mesher = new this.occt.BRepMesh_IncrementalMesh(shape, deflection, false, angle);
      mesher.Perform();

      const location = new this.occt.TopLoc_Location();

      const positions: number[] = [];
      const normals: number[] = [];
      const indices: number[] = [];

      const explorer = new this.occt.TopExp_Explorer(shape, this.occt.TopAbs_FACE);
      let indexOffset = 0;

      while (explorer.More()) {
        const face = this.occt.TopoDS.Face(explorer.Current());
        const tri = this.occt.BRep_Tool.Triangulation(face, location);

        if (tri && !tri.IsNull()) {
          const nodes = tri.Nodes();
          const triangles = tri.Triangles();

          for (let i = 1; i <= nodes.Length(); i++) {
            const node = nodes.Value(i);
            positions.push(node.X!(), node.Y!(), node.Z!());
            normals.push(0, 0, 1); // Simplified - should compute from face
          }

          for (let i = 1; i <= triangles.Length(); i++) {
            const triangle = triangles.Value(i);
            indices.push(
              triangle.Value(1) - 1 + indexOffset,
              triangle.Value(2) - 1 + indexOffset,
              triangle.Value(3) - 1 + indexOffset
            );
          }

          indexOffset += nodes.Length();
        }

        explorer.Next();
      }

      mesher.delete();
      location.delete();
      explorer.delete();

      return {
        positions: new Float32Array(positions),
        normals: new Float32Array(normals),
        indices: new Uint32Array(indices),
      };
    }, this.logger);
  }

  getProperties(shapeOrId: string | ShapeHandle): OperationResult<ShapeProperties> {
    return timed('getProperties', () => {
      const shape = this.shapeUtils.getShape(shapeOrId);
      if (!shape) throw new Error('Shape not found');

      const props = new this.occt.GProp_GProps();
      const calculator = new this.occt.BRepGProp();

      calculator.VolumeProperties(shape, props);

      const volume = (props as any).Mass();
      const center = (props as any).CentreOfMass();

      // Get surface area
      calculator.SurfaceProperties(shape, props);
      const surfaceArea = (props as any).Mass();

      const result: ShapeProperties = {
        volume,
        surfaceArea,
        centerOfMass: {
          x: center.X(),
          y: center.Y(),
          z: center.Z(),
        },
        boundingBox: this.shapeUtils.calculateBounds(shape),
      };

      props.delete();
      calculator.delete();

      return result;
    }, this.logger);
  }

  getVolume(shapeOrId: string | ShapeHandle): OperationResult<number> {
    return timed('getVolume', () => {
      const shape = this.shapeUtils.getShape(shapeOrId);
      if (!shape) throw new Error('Shape not found');

      const props = new this.occt.GProp_GProps();
      const calculator = new this.occt.BRepGProp();

      calculator.VolumeProperties(shape, props);
      const volume = (props as any).Mass();

      props.delete();
      calculator.delete();

      return volume;
    }, this.logger);
  }

  getSurfaceArea(shapeOrId: string | ShapeHandle): OperationResult<number> {
    return timed('getSurfaceArea', () => {
      const shape = this.shapeUtils.getShape(shapeOrId);
      if (!shape) throw new Error('Shape not found');

      const props = new this.occt.GProp_GProps();
      const calculator = new this.occt.BRepGProp();

      calculator.SurfaceProperties(shape, props);
      const area = (props as any).Mass();

      props.delete();
      calculator.delete();

      return area;
    }, this.logger);
  }

  getBoundingBox(shapeOrId: string | ShapeHandle): OperationResult<BoundingBox> {
    return timed('getBoundingBox', () => {
      const shape = this.shapeUtils.getShape(shapeOrId);
      if (!shape) throw new Error('Shape not found');

      return this.shapeUtils.calculateBounds(shape);
    }, this.logger);
  }

  // ===========================================================================
  // Zero-Lag Optimization
  // ===========================================================================

  /**
   * Estimate the complexity of an operation before executing it.
   * Used for deciding whether to run locally or offload to remote compute.
   */
  estimateComplexity(
    operation: string,
    shapeIds: string[]
  ): ComplexityEstimate {
    // Simple heuristic based on operation type and shape count
    const operationWeights: Record<string, number> = {
      // Instant operations (<16ms)
      makeBox: 0.05,
      makeSphere: 0.05,
      makeCylinder: 0.05,
      makeCone: 0.05,
      makeTorus: 0.05,
      translate: 0.02,
      rotate: 0.02,
      scale: 0.02,
      mirror: 0.02,

      // Interactive operations (<100ms)
      booleanUnion: 0.3,
      booleanSubtract: 0.35,
      booleanIntersect: 0.3,
      extrude: 0.15,
      revolve: 0.2,

      // Background operations (<1s)
      fillet: 0.5,
      chamfer: 0.4,
      shell: 0.6,
      sweep: 0.5,
      loft: 0.6,
      offset: 0.5,

      // Heavy operations (>1s) - consider remote
      tessellate: 0.4, // Depends heavily on mesh density
    };

    const baseWeight = operationWeights[operation] || 0.5;
    const shapeCount = shapeIds.length;

    // Increase complexity with more shapes
    const score = Math.min(1, baseWeight * (1 + shapeCount * 0.1));

    // Rough time estimates
    const estimatedMs = score < 0.1 ? 5
                      : score < 0.3 ? 50
                      : score < 0.6 ? 200
                      : score < 0.8 ? 500
                      : 2000;

    // Memory estimate (rough)
    const estimatedBytes = shapeCount * 50000 * score;

    // Recommend remote for heavy operations
    const recommendedLocation: ComputeLocation = score > 0.7 ? 'remote' : 'local';

    return {
      score,
      estimatedMs,
      estimatedBytes,
      recommendedLocation,
    };
  }

  /**
   * Precompute hints for speculative execution.
   * Call this when user hovers over tools to warm up the cache.
   */
  precompute(operation: string, shapeIds: string[]): void {
    // This would trigger background computation in a real implementation
    this.logger.debug(`Precompute hint: ${operation} with ${shapeIds.length} shapes`);
  }
}

// =============================================================================
// Factory Function
// =============================================================================

export function createGeometryEngine(occt: WASMModule): GeometryEngine {
  return new GeometryEngine(occt);
}
