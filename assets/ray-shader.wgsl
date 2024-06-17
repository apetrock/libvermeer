struct Uniforms {
    orientation: vec4<f32>,
    width: u32,
    height: u32,
    p0: f32,
    p1: f32
}

struct ray {
    origin: vec3<f32>,
    pad: u32, 
    direction: vec3<f32>,
    id: u32,     
}

struct Sphere {
    pos      : vec3<f32>,
    radius   : f32,
    color    : vec3<f32>
}

struct hitResult {
    t      : f32, // negative for no hit
    normal : vec3<f32>,
    pnt    : vec3<f32>,
    color  : vec3<f32>
}

// 'let' will be changed to 'const' in future (left for compatability reasons)
const spheres = array<Sphere, 4>( Sphere( vec3<f32>(-0.3, -0.1, 1.0 ), 0.2, vec3<f32>(1.0,0.0,0.0) ),
                                  Sphere( vec3<f32>( -0.2, 0.5, 1.0 ), 0.1, vec3<f32>(0.0,1.0,0.0) ),
                                  Sphere( vec3<f32>( 0.4, -0.1, 1.0),  0.4, vec3<f32>(0.0,0.0,1.0) ),
                                  Sphere( vec3<f32>( 0.2, 0.8,  1.0 ), 0.3, vec3<f32>(1.0,0.0,1.0) ) 
                              );

fn lineIntersectSphere( origin:vec3<f32>, dir:vec3<f32>, cen:vec3<f32>, r:f32 ) -> hitResult
{
    var res = hitResult(-1.0, vec3<f32>(0.0), vec3<f32>(0.0), vec3<f32>(0.0) );
    let L = cen - origin;
    let dir = normalize(dir);

    let tca = dot(dir, L);

    if(tca < 0f){
        return res;
    }
    
    let d2 = dot(L,L) - tca * tca;

    if(d2 > r*r){
        return res;
    }

    let thc = sqrt(r*r - d2);
    let t0 = tca - thc;
    let t1 = tca + thc;
    
    var t = min(t0, t1);
    t = select(t, t1, t < 0f);

    let p = origin + t0 * dir;
    res.normal = normalize(p - cen);
    res.pnt = p;
    res.t = t;

    return res; // hit!!
}

fn getSphere(i: u32) -> Sphere{
    var sphere = Sphere(vec3<f32>(0.0), 0.0, vec3<f32>(0.0));
    if(i==0u){
        sphere = spheres[0];
    }
    if(i==1u){
        sphere = spheres[1];
    }
    if(i==2u){
        sphere = spheres[2];
    }
    if(i==3u){
        sphere = spheres[3];
    }

    return sphere;
}

fn sceneIntersect(origin:vec3<f32>, dir:vec3<f32>) -> hitResult
{
    var sceneRes = hitResult(-1.0, vec3<f32>(0.0), vec3<f32>(0.0), vec3<f32>(0.0) );
    for (var i:u32 = 0u; i < 4u ; i++)
    {
    	var sphere = getSphere(i);
            
    	let res = lineIntersectSphere( origin, dir, sphere.pos, sphere.radius );
        if ( res.t > 0.0 )
    	{
            if ( sceneRes.t > 0.0 ) 
            { 
                if ( res.t > sceneRes.t ) { continue; }
            }
            sceneRes        = res;
            sceneRes.color  = sphere.color;
        }
    }
    return sceneRes;
}

@group(0) @binding(0) var inputTexture: texture_2d<f32>;
@group(0) @binding(1) var outputTexture: texture_storage_2d<rgba8unorm,write>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;
@group(0) @binding(3) var<storage,read_write> rayPool: array<ray>;

@compute @workgroup_size(8, 8)
fn trace(@builtin(global_invocation_id) id: vec3<u32>) {

    let width = uniforms.width;
    let height = uniforms.height;
    let idx = id.x + width*id.y;
    //let color = textureLoad(inputTexture, id.xy, 0).rgb;
    //let color = rayPool[idx].direction.xyz;
    let ridx = rayPool[idx].id;
    let rid = vec2<u32>(ridx % width, ridx/width);
    let fx = f32(rid.x - id.x) / f32(width);
    let fy = f32(rid.y - id.y) / f32(height);
    let color = vec3<f32>(fx, fy, 0.0);
    let origin = rayPool[idx].origin;
    let dir = rayPool[idx].direction;
    //initilize colors
    var fragColor:vec3<f32> = vec3<f32>(0.0); // no-hit - white
    var lightPos            = vec3<f32>(2f,4f,-3f);
    
    let res = sceneIntersect( origin, dir );
    //fragColor = vec3<f32>(dir);
    if ( res.t > 0f )
    {
        //fragColor = vec3<f32>(res.t);
        
        var hitColor:vec3<f32> = res.color;

        var ambient = vec3<f32>(0.025);

        var lightDir = normalize( lightPos - res.pnt );
        var diffuse = ( dot( res.normal, lightDir ) );
        diffuse = clamp( diffuse, 0f, 1f);

        var v = normalize( origin - res.pnt );
        var shinnyfactor = 4.0;
        var h = (lightDir+v)*0.5;
        var hdotn = dot(h,res.normal);
        var specular = pow(hdotn, shinnyfactor );

        fragColor = ambient +
            		hitColor * diffuse +
            		hitColor * specular;
        
        
        // SHADOW RAY-CALCULATION
        var shadow = 1.0; // start with no shadow value
        var pnt2 = res.pnt + res.normal*0.1; // shoot ray to light
        var res2 = sceneIntersect( pnt2, lightDir );
        if ( res2.t > 0.0 )
        {   // hit something, is it between us and the light?
            let d1 = length( res2.pnt - res.pnt );
            let d2 = length( lightPos - res.pnt );
            if ( d1 < d2 )
            {
                shadow = 0.1; // in shadow!!!
            }
        }
        fragColor *= shadow;
        
    }
    
    textureStore(outputTexture, id.xy, vec4<f32>(fragColor, 1.0));
}
